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

#include <graphics/vec4.h>
#include <float.h>

#include "d3d11-subsystem.hpp"

static inline D3D11_TEXTURE_ADDRESS_MODE
ConvertGSAddressMode(gs_address_mode mode)
{
	switch (mode) {
	case GS_ADDRESS_WRAP:
		return D3D11_TEXTURE_ADDRESS_WRAP;
	case GS_ADDRESS_CLAMP:
		return D3D11_TEXTURE_ADDRESS_CLAMP;
	case GS_ADDRESS_MIRROR:
		return D3D11_TEXTURE_ADDRESS_MIRROR;
	case GS_ADDRESS_BORDER:
		return D3D11_TEXTURE_ADDRESS_BORDER;
	case GS_ADDRESS_MIRRORONCE:
		return D3D11_TEXTURE_ADDRESS_MIRROR_ONCE;
	}

	return D3D11_TEXTURE_ADDRESS_WRAP;
}

static inline D3D11_FILTER ConvertGSFilter(gs_sample_filter filter)
{
	switch (filter) {
	case GS_FILTER_POINT:
		return D3D11_FILTER_MIN_MAG_MIP_POINT;
	case GS_FILTER_LINEAR:
		return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	case GS_FILTER_MIN_MAG_POINT_MIP_LINEAR:
		return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
	case GS_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:
		return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
	case GS_FILTER_MIN_POINT_MAG_MIP_LINEAR:
		return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
	case GS_FILTER_MIN_LINEAR_MAG_MIP_POINT:
		return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
	case GS_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
		return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
	case GS_FILTER_MIN_MAG_LINEAR_MIP_POINT:
		return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	case GS_FILTER_ANISOTROPIC:
		return D3D11_FILTER_ANISOTROPIC;
	}

	return D3D11_FILTER_MIN_MAG_MIP_POINT;
}

gs_sampler_state::gs_sampler_state(gs_device_t *device,
				   const gs_sampler_info *info)
	: gs_obj(device, gs_type::gs_sampler_state), info(*info)
{
	HRESULT hr;
	vec4 v4;

	memset(&sd, 0, sizeof(sd));
	sd.AddressU = ConvertGSAddressMode(info->address_u);
	sd.AddressV = ConvertGSAddressMode(info->address_v);
	sd.AddressW = ConvertGSAddressMode(info->address_w);
	sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sd.Filter = ConvertGSFilter(info->filter);
	sd.MaxAnisotropy = info->max_anisotropy;
	sd.MaxLOD = FLT_MAX;

	vec4_from_rgba(&v4, info->border_color);
	memcpy(sd.BorderColor, v4.ptr, sizeof(v4));

	hr = device->device->CreateSamplerState(&sd, state.Assign());
	if (FAILED(hr))
		throw HRError("Failed to create sampler state", hr);
}
