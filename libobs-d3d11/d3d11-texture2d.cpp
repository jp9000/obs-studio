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

#include <util/base.h>
#include "d3d11-subsystem.hpp"

void gs_texture_2d::InitSRD(vector<D3D11_SUBRESOURCE_DATA> &srd)
{
	uint32_t rowSizeBytes = width * gs_get_format_bpp(format);
	uint32_t texSizeBytes = height * rowSizeBytes / 8;
	size_t textures = type == GS_TEXTURE_2D ? 1 : 6;
	uint32_t actual_levels = levels;
	size_t curTex = 0;

	if (!actual_levels)
		actual_levels = gs_get_total_levels(width, height, 1);

	rowSizeBytes /= 8;

	for (size_t i = 0; i < textures; i++) {
		uint32_t newRowSize = rowSizeBytes;
		uint32_t newTexSize = texSizeBytes;

		for (uint32_t j = 0; j < actual_levels; j++) {
			D3D11_SUBRESOURCE_DATA newSRD;
			newSRD.pSysMem = data[curTex++].data();
			newSRD.SysMemPitch = newRowSize;
			newSRD.SysMemSlicePitch = newTexSize;
			srd.push_back(newSRD);

			newRowSize /= 2;
			newTexSize /= 4;
		}
	}
}

void gs_texture_2d::BackupTexture(const uint8_t *const *data)
{
	this->data.resize(levels);

	uint32_t w = width;
	uint32_t h = height;
	uint32_t bbp = gs_get_format_bpp(format);

	for (uint32_t i = 0; i < levels; i++) {
		if (!data[i])
			break;

		uint32_t texSize = bbp * w * h / 8;
		this->data[i].resize(texSize);

		vector<uint8_t> &subData = this->data[i];
		memcpy(&subData[0], data[i], texSize);

		if (w > 1)
			w /= 2;
		if (h > 1)
			h /= 2;
	}
}

void gs_texture_2d::GetSharedHandle(IDXGIResource *dxgi_res)
{
	HANDLE handle;
	HRESULT hr;

	hr = dxgi_res->GetSharedHandle(&handle);
	if (FAILED(hr)) {
		blog(LOG_WARNING,
		     "GetSharedHandle: Failed to "
		     "get shared handle: %08lX",
		     hr);
	} else {
		sharedHandle = (uint32_t)(uintptr_t)handle;
	}
}

void gs_texture_2d::InitTexture(const uint8_t *const *data)
{
	HRESULT hr;

	memset(&td, 0, sizeof(td));
	td.Width = width;
	td.Height = height;
	td.MipLevels = genMipmaps ? 0 : levels;
	td.ArraySize = type == GS_TEXTURE_CUBE ? 6 : 1;
	td.Format = nv12 ? DXGI_FORMAT_NV12 : dxgiFormatResource;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.SampleDesc.Count = 1;
	td.CPUAccessFlags = isDynamic ? D3D11_CPU_ACCESS_WRITE : 0;
	td.Usage = isDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;

	if (type == GS_TEXTURE_CUBE)
		td.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

	if (isRenderTarget || isGDICompatible)
		td.BindFlags |= D3D11_BIND_RENDER_TARGET;

	if (isGDICompatible)
		td.MiscFlags |= D3D11_RESOURCE_MISC_GDI_COMPATIBLE;

	if ((flags & GS_SHARED_KM_TEX) != 0)
		td.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
	else if ((flags & GS_SHARED_TEX) != 0)
		td.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;

	if (data) {
		BackupTexture(data);
		InitSRD(srd);
	}

	hr = device->device->CreateTexture2D(&td, data ? srd.data() : NULL,
					     texture.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create 2D texture", hr);

	if (isGDICompatible) {
		hr = texture->QueryInterface(__uuidof(IDXGISurface1),
					     (void **)gdiSurface.Assign());
		if (FAILED(hr))
			throw HRError("Failed to create GDI surface", hr);
	}

	if (isShared) {
		ComPtr<IDXGIResource> dxgi_res;

		texture->SetEvictionPriority(DXGI_RESOURCE_PRIORITY_MAXIMUM);

		hr = texture->QueryInterface(__uuidof(IDXGIResource),
					     (void **)&dxgi_res);
		if (FAILED(hr)) {
			blog(LOG_WARNING,
			     "InitTexture: Failed to query "
			     "interface: %08lX",
			     hr);
		} else {
			GetSharedHandle(dxgi_res);

			if (flags & GS_SHARED_KM_TEX) {
				ComPtr<IDXGIKeyedMutex> km;
				hr = texture->QueryInterface(
					__uuidof(IDXGIKeyedMutex),
					(void **)&km);
				if (FAILED(hr)) {
					throw HRError("Failed to query "
						      "IDXGIKeyedMutex",
						      hr);
				}

				km->AcquireSync(0, INFINITE);
				acquired = true;
			}
		}
	}
}

void gs_texture_2d::InitResourceView()
{
	HRESULT hr;

	memset(&viewDesc, 0, sizeof(viewDesc));
	viewDesc.Format = dxgiFormatView;

	if (type == GS_TEXTURE_CUBE) {
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		viewDesc.TextureCube.MipLevels = genMipmaps || !levels ? -1
								       : levels;
	} else {
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = genMipmaps || !levels ? -1
								     : levels;
	}

	hr = device->device->CreateShaderResourceView(texture, &viewDesc,
						      shaderRes.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create SRV", hr);

	viewDescLinear = viewDesc;
	viewDescLinear.Format = dxgiFormatViewLinear;

	if (dxgiFormatView == dxgiFormatViewLinear) {
		shaderResLinear = shaderRes;
	} else {
		hr = device->device->CreateShaderResourceView(
			texture, &viewDescLinear, shaderResLinear.Assign());
		if (FAILED(hr))
			throw HRError("Failed to create linear SRV", hr);
	}
}

void gs_texture_2d::InitRenderTargets()
{
	HRESULT hr;
	if (type == GS_TEXTURE_2D) {
		D3D11_RENDER_TARGET_VIEW_DESC rtv;
		rtv.Format = dxgiFormatView;
		rtv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtv.Texture2D.MipSlice = 0;

		hr = device->device->CreateRenderTargetView(
			texture, &rtv, renderTarget[0].Assign());
		if (FAILED(hr))
			throw HRError("Failed to create RTV", hr);
		if (dxgiFormatView == dxgiFormatViewLinear) {
			renderTargetLinear[0] = renderTarget[0];
		} else {
			rtv.Format = dxgiFormatViewLinear;
			hr = device->device->CreateRenderTargetView(
				texture, &rtv, renderTargetLinear[0].Assign());
			if (FAILED(hr))
				throw HRError("Failed to create linear RTV",
					      hr);
		}
	} else {
		D3D11_RENDER_TARGET_VIEW_DESC rtv;
		rtv.Format = dxgiFormatView;
		rtv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		rtv.Texture2DArray.MipSlice = 0;
		rtv.Texture2DArray.ArraySize = 1;

		for (UINT i = 0; i < 6; i++) {
			rtv.Texture2DArray.FirstArraySlice = i;
			hr = device->device->CreateRenderTargetView(
				texture, &rtv, renderTarget[i].Assign());
			if (FAILED(hr))
				throw HRError("Failed to create cube RTV", hr);
			if (dxgiFormatView == dxgiFormatViewLinear) {
				renderTargetLinear[i] = renderTarget[i];
			} else {
				rtv.Format = dxgiFormatViewLinear;
				hr = device->device->CreateRenderTargetView(
					texture, &rtv,
					renderTargetLinear[i].Assign());
				if (FAILED(hr))
					throw HRError(
						"Failed to create linear cube RTV",
						hr);
			}
		}
	}
}

#define SHARED_FLAGS (GS_SHARED_TEX | GS_SHARED_KM_TEX)

gs_texture_2d::gs_texture_2d(gs_device_t *device, uint32_t width,
			     uint32_t height, gs_color_format colorFormat,
			     uint32_t levels, const uint8_t *const *data,
			     uint32_t flags_, gs_texture_type type,
			     bool gdiCompatible, bool nv12_)
	: gs_texture(device, gs_type::gs_texture_2d, type, levels, colorFormat),
	  width(width),
	  height(height),
	  flags(flags_),
	  dxgiFormatResource(ConvertGSTextureFormatResource(format)),
	  dxgiFormatView(ConvertGSTextureFormatView(format)),
	  dxgiFormatViewLinear(ConvertGSTextureFormatViewLinear(format)),
	  isRenderTarget((flags_ & GS_RENDER_TARGET) != 0),
	  isGDICompatible(gdiCompatible),
	  isDynamic((flags_ & GS_DYNAMIC) != 0),
	  isShared((flags_ & SHARED_FLAGS) != 0),
	  genMipmaps((flags_ & GS_BUILD_MIPMAPS) != 0),
	  sharedHandle(GS_INVALID_HANDLE),
	  nv12(nv12_)
{
	InitTexture(data);
	InitResourceView();

	if (isRenderTarget)
		InitRenderTargets();
}

gs_texture_2d::gs_texture_2d(gs_device_t *device, ID3D11Texture2D *nv12tex,
			     uint32_t flags_)
	: gs_texture(device, gs_type::gs_texture_2d, GS_TEXTURE_2D),
	  isRenderTarget((flags_ & GS_RENDER_TARGET) != 0),
	  isDynamic((flags_ & GS_DYNAMIC) != 0),
	  isShared((flags_ & SHARED_FLAGS) != 0),
	  genMipmaps((flags_ & GS_BUILD_MIPMAPS) != 0),
	  nv12(true),
	  texture(nv12tex)
{
	texture->GetDesc(&td);

	this->type = GS_TEXTURE_2D;
	this->format = GS_R8G8;
	this->flags = flags_;
	this->levels = 1;
	this->device = device;
	this->chroma = true;
	this->width = td.Width / 2;
	this->height = td.Height / 2;
	this->dxgiFormatResource = DXGI_FORMAT_R8G8_UNORM;
	this->dxgiFormatView = DXGI_FORMAT_R8G8_UNORM;
	this->dxgiFormatViewLinear = DXGI_FORMAT_R8G8_UNORM;

	InitResourceView();
	if (isRenderTarget)
		InitRenderTargets();
}

gs_texture_2d::gs_texture_2d(gs_device_t *device, uint32_t handle)
	: gs_texture(device, gs_type::gs_texture_2d, GS_TEXTURE_2D),
	  isShared(true),
	  sharedHandle(handle)
{
	HRESULT hr;
	hr = device->device->OpenSharedResource((HANDLE)(uintptr_t)handle,
						__uuidof(ID3D11Texture2D),
						(void **)texture.Assign());
	if (FAILED(hr))
		throw HRError("Failed to open shared 2D texture", hr);

	texture->GetDesc(&td);

	const gs_color_format format = ConvertDXGITextureFormat(td.Format);

	this->type = GS_TEXTURE_2D;
	this->format = format;
	this->levels = 1;
	this->device = device;

	this->width = td.Width;
	this->height = td.Height;
	this->dxgiFormatResource = ConvertGSTextureFormatResource(format);
	this->dxgiFormatView = ConvertGSTextureFormatView(format);
	this->dxgiFormatViewLinear = ConvertGSTextureFormatViewLinear(format);

	InitResourceView();
}

gs_texture_2d::gs_texture_2d(gs_device_t *device, ID3D11Texture2D *obj)
	: gs_texture(device, gs_type::gs_texture_2d, GS_TEXTURE_2D)
{
	texture = obj;

	texture->GetDesc(&td);

	const gs_color_format format = ConvertDXGITextureFormat(td.Format);

	this->type = GS_TEXTURE_2D;
	this->format = format;
	this->levels = 1;
	this->device = device;

	this->width = td.Width;
	this->height = td.Height;
	this->dxgiFormatResource = ConvertGSTextureFormatResource(format);
	this->dxgiFormatView = ConvertGSTextureFormatView(format);
	this->dxgiFormatViewLinear = ConvertGSTextureFormatViewLinear(format);

	InitResourceView();
}
