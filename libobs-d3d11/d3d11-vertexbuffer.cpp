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
#include <graphics/vec3.h>
#include "d3d11-subsystem.hpp"

static inline void PushBuffer(vector<ID3D11Buffer *> &buffers,
			      vector<uint32_t> &strides, ID3D11Buffer *buffer,
			      size_t elementSize, const char *name)
{
	if (buffer) {
		buffers.push_back(buffer);
		strides.push_back((uint32_t)elementSize);
	} else {
		blog(LOG_ERROR, "This vertex shader requires a %s buffer",
		     name);
	}
}

void gs_vertex_buffer::FlushBuffer(ID3D11Buffer *buffer, void *array,
				   size_t elementSize)
{
	D3D11_MAPPED_SUBRESOURCE msr;
	HRESULT hr;

	if (FAILED(hr = device->context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD,
					     0, &msr)))
		throw HRError("Failed to map buffer", hr);

	memcpy(msr.pData, array, elementSize * vbd.data->num);
	device->context->Unmap(buffer, 0);
}

void gs_vertex_buffer::MakeBufferList(gs_vertex_shader *shader,
				      vector<ID3D11Buffer *> &buffers,
				      vector<uint32_t> &strides)
{
	PushBuffer(buffers, strides, vertexBuffer, sizeof(vec3), "point");

	if (shader->hasNormals)
		PushBuffer(buffers, strides, normalBuffer, sizeof(vec3),
			   "normal");
	if (shader->hasColors)
		PushBuffer(buffers, strides, colorBuffer, sizeof(uint32_t),
			   "color");
	if (shader->hasTangents)
		PushBuffer(buffers, strides, tangentBuffer, sizeof(vec3),
			   "tangent");
	if (shader->nTexUnits <= uvBuffers.size()) {
		for (size_t i = 0; i < shader->nTexUnits; i++) {
			buffers.push_back(uvBuffers[i]);
			strides.push_back((uint32_t)uvSizes[i]);
		}
	} else {
		blog(LOG_ERROR,
		     "This vertex shader requires at least %u "
		     "texture buffers.",
		     (uint32_t)shader->nTexUnits);
	}
}

void gs_vertex_buffer::InitBuffer(const size_t elementSize,
				  const size_t numVerts, void *array,
				  ID3D11Buffer **buffer)
{
	D3D11_BUFFER_DESC bd;
	D3D11_SUBRESOURCE_DATA srd;
	HRESULT hr;

	memset(&bd, 0, sizeof(bd));
	memset(&srd, 0, sizeof(srd));

	bd.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
	bd.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.ByteWidth = UINT(elementSize * numVerts);
	srd.pSysMem = array;

	hr = device->device->CreateBuffer(&bd, &srd, buffer);
	if (FAILED(hr))
		throw HRError("Failed to create buffer", hr);
}

void gs_vertex_buffer::BuildBuffers()
{
	InitBuffer(sizeof(vec3), vbd.data->num, vbd.data->points,
		   &vertexBuffer);

	if (vbd.data->normals)
		InitBuffer(sizeof(vec3), vbd.data->num, vbd.data->normals,
			   &normalBuffer);

	if (vbd.data->tangents)
		InitBuffer(sizeof(vec3), vbd.data->num, vbd.data->tangents,
			   &tangentBuffer);

	if (vbd.data->colors)
		InitBuffer(sizeof(uint32_t), vbd.data->num, vbd.data->colors,
			   &colorBuffer);

	for (size_t i = 0; i < vbd.data->num_tex; i++) {
		struct gs_tvertarray *tverts = vbd.data->tvarray + i;

		if (tverts->width != 2 && tverts->width != 4)
			throw "Invalid texture vertex size specified";
		if (!tverts->array)
			throw "No texture vertices specified";

		ComPtr<ID3D11Buffer> buffer;
		InitBuffer(tverts->width * sizeof(float), vbd.data->num,
			   tverts->array, &buffer);

		uvBuffers.push_back(buffer);
		uvSizes.push_back(tverts->width * sizeof(float));
	}
}

gs_vertex_buffer::gs_vertex_buffer(gs_device_t *device, struct gs_vb_data *data,
				   uint32_t flags)
	: gs_obj(device, gs_type::gs_vertex_buffer),
	  dynamic((flags & GS_DYNAMIC) != 0),
	  vbd(data),
	  numVerts(data->num)
{
	if (!data->num)
		throw "Cannot initialize vertex buffer with 0 vertices";
	if (!data->points)
		throw "No points specified for vertex buffer";

	BuildBuffers();
}
