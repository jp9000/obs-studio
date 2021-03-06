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

#include "d3d11-subsystem.hpp"

void gs_zstencil_buffer::InitBuffer()
{
	HRESULT hr;

	memset(&td, 0, sizeof(td));
	td.Width = width;
	td.Height = height;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = dxgiFormat;
	td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;

	hr = device->device->CreateTexture2D(&td, NULL, texture.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create depth stencil texture", hr);

	memset(&dsvd, 0, sizeof(dsvd));
	dsvd.Format = dxgiFormat;
	dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

	hr = device->device->CreateDepthStencilView(texture, &dsvd,
						    view.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create depth stencil view", hr);
}

gs_zstencil_buffer::gs_zstencil_buffer(gs_device_t *device, uint32_t width,
				       uint32_t height,
				       gs_zstencil_format format)
	: gs_obj(device, gs_type::gs_zstencil_buffer),
	  width(width),
	  height(height),
	  format(format),
	  dxgiFormat(ConvertGSZStencilFormat(format))
{
	InitBuffer();
}
