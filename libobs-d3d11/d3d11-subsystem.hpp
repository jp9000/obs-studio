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

#pragma once

#include <util/AlignedNew.hpp>
#include <util/windows/win-version.h>

#include <vector>
#include <string>
#include <memory>

#include <windows.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>

#include <util/base.h>
#include <graphics/matrix4.h>
#include <graphics/graphics.h>
#include <graphics/device-exports.h>
#include <util/windows/ComPtr.hpp>
#include <util/windows/HRError.hpp>

// #define DISASSEMBLE_SHADERS

struct shader_var;
struct shader_sampler;
struct gs_vertex_shader;

using namespace std;

/*
 * Just to clarify, all structs, and all public.  These are exporting only
 * via encapsulated C bindings, not C++ bindings, so the whole concept of
 * "public" and "private" does not matter at all for this subproject.
 */

static inline uint32_t GetWinVer()
{
	struct win_version_info ver;
	get_win_ver(&ver);

	return (ver.major << 8) | ver.minor;
}

static inline DXGI_FORMAT ConvertGSTextureFormatResource(gs_color_format format)
{
	switch (format) {
	case GS_UNKNOWN:
		return DXGI_FORMAT_UNKNOWN;
	case GS_A8:
		return DXGI_FORMAT_A8_UNORM;
	case GS_R8:
		return DXGI_FORMAT_R8_UNORM;
	case GS_RGBA:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;
	case GS_BGRX:
		return DXGI_FORMAT_B8G8R8X8_TYPELESS;
	case GS_BGRA:
		return DXGI_FORMAT_B8G8R8A8_TYPELESS;
	case GS_R10G10B10A2:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	case GS_RGBA16:
		return DXGI_FORMAT_R16G16B16A16_UNORM;
	case GS_R16:
		return DXGI_FORMAT_R16_UNORM;
	case GS_RGBA16F:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case GS_RGBA32F:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case GS_RG16F:
		return DXGI_FORMAT_R16G16_FLOAT;
	case GS_RG32F:
		return DXGI_FORMAT_R32G32_FLOAT;
	case GS_R16F:
		return DXGI_FORMAT_R16_FLOAT;
	case GS_R32F:
		return DXGI_FORMAT_R32_FLOAT;
	case GS_DXT1:
		return DXGI_FORMAT_BC1_UNORM;
	case GS_DXT3:
		return DXGI_FORMAT_BC2_UNORM;
	case GS_DXT5:
		return DXGI_FORMAT_BC3_UNORM;
	case GS_R8G8:
		return DXGI_FORMAT_R8G8_UNORM;
	case GS_RGBA_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case GS_BGRX_UNORM:
		return DXGI_FORMAT_B8G8R8X8_UNORM;
	case GS_BGRA_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	}

	return DXGI_FORMAT_UNKNOWN;
}

static inline DXGI_FORMAT ConvertGSTextureFormatView(gs_color_format format)
{
	switch (format) {
	case GS_RGBA:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case GS_BGRX:
		return DXGI_FORMAT_B8G8R8X8_UNORM;
	case GS_BGRA:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	default:
		return ConvertGSTextureFormatResource(format);
	}
}

static inline DXGI_FORMAT
ConvertGSTextureFormatViewLinear(gs_color_format format)
{
	switch (format) {
	case GS_RGBA:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case GS_BGRX:
		return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
	case GS_BGRA:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	default:
		return ConvertGSTextureFormatResource(format);
	}
}

static inline gs_color_format ConvertDXGITextureFormat(DXGI_FORMAT format)
{
	switch ((unsigned long)format) {
	case DXGI_FORMAT_A8_UNORM:
		return GS_A8;
	case DXGI_FORMAT_R8_UNORM:
		return GS_R8;
	case DXGI_FORMAT_R8G8_UNORM:
		return GS_R8G8;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		return GS_RGBA;
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		return GS_BGRX;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		return GS_BGRA;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
		return GS_R10G10B10A2;
	case DXGI_FORMAT_R16G16B16A16_UNORM:
		return GS_RGBA16;
	case DXGI_FORMAT_R16_UNORM:
		return GS_R16;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return GS_RGBA16F;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return GS_RGBA32F;
	case DXGI_FORMAT_R16G16_FLOAT:
		return GS_RG16F;
	case DXGI_FORMAT_R32G32_FLOAT:
		return GS_RG32F;
	case DXGI_FORMAT_R16_FLOAT:
		return GS_R16F;
	case DXGI_FORMAT_R32_FLOAT:
		return GS_R32F;
	case DXGI_FORMAT_BC1_UNORM:
		return GS_DXT1;
	case DXGI_FORMAT_BC2_UNORM:
		return GS_DXT3;
	case DXGI_FORMAT_BC3_UNORM:
		return GS_DXT5;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return GS_RGBA_UNORM;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return GS_BGRX_UNORM;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return GS_BGRA_UNORM;
	}

	return GS_UNKNOWN;
}

static inline DXGI_FORMAT ConvertGSZStencilFormat(gs_zstencil_format format)
{
	switch (format) {
	case GS_ZS_NONE:
		return DXGI_FORMAT_UNKNOWN;
	case GS_Z16:
		return DXGI_FORMAT_D16_UNORM;
	case GS_Z24_S8:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case GS_Z32F:
		return DXGI_FORMAT_D32_FLOAT;
	case GS_Z32F_S8X24:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	}

	return DXGI_FORMAT_UNKNOWN;
}

static inline D3D11_COMPARISON_FUNC ConvertGSDepthTest(gs_depth_test test)
{
	switch (test) {
	case GS_NEVER:
		return D3D11_COMPARISON_NEVER;
	case GS_LESS:
		return D3D11_COMPARISON_LESS;
	case GS_LEQUAL:
		return D3D11_COMPARISON_LESS_EQUAL;
	case GS_EQUAL:
		return D3D11_COMPARISON_EQUAL;
	case GS_GEQUAL:
		return D3D11_COMPARISON_GREATER_EQUAL;
	case GS_GREATER:
		return D3D11_COMPARISON_GREATER;
	case GS_NOTEQUAL:
		return D3D11_COMPARISON_NOT_EQUAL;
	case GS_ALWAYS:
		return D3D11_COMPARISON_ALWAYS;
	}

	return D3D11_COMPARISON_NEVER;
}

static inline D3D11_STENCIL_OP ConvertGSStencilOp(gs_stencil_op_type op)
{
	switch (op) {
	case GS_KEEP:
		return D3D11_STENCIL_OP_KEEP;
	case GS_ZERO:
		return D3D11_STENCIL_OP_ZERO;
	case GS_REPLACE:
		return D3D11_STENCIL_OP_REPLACE;
	case GS_INCR:
		return D3D11_STENCIL_OP_INCR;
	case GS_DECR:
		return D3D11_STENCIL_OP_DECR;
	case GS_INVERT:
		return D3D11_STENCIL_OP_INVERT;
	}

	return D3D11_STENCIL_OP_KEEP;
}

static inline D3D11_BLEND ConvertGSBlendType(gs_blend_type type)
{
	switch (type) {
	case GS_BLEND_ZERO:
		return D3D11_BLEND_ZERO;
	case GS_BLEND_ONE:
		return D3D11_BLEND_ONE;
	case GS_BLEND_SRCCOLOR:
		return D3D11_BLEND_SRC_COLOR;
	case GS_BLEND_INVSRCCOLOR:
		return D3D11_BLEND_INV_SRC_COLOR;
	case GS_BLEND_SRCALPHA:
		return D3D11_BLEND_SRC_ALPHA;
	case GS_BLEND_INVSRCALPHA:
		return D3D11_BLEND_INV_SRC_ALPHA;
	case GS_BLEND_DSTCOLOR:
		return D3D11_BLEND_DEST_COLOR;
	case GS_BLEND_INVDSTCOLOR:
		return D3D11_BLEND_INV_DEST_COLOR;
	case GS_BLEND_DSTALPHA:
		return D3D11_BLEND_DEST_ALPHA;
	case GS_BLEND_INVDSTALPHA:
		return D3D11_BLEND_INV_DEST_ALPHA;
	case GS_BLEND_SRCALPHASAT:
		return D3D11_BLEND_SRC_ALPHA_SAT;
	}

	return D3D11_BLEND_ONE;
}

static inline D3D11_CULL_MODE ConvertGSCullMode(gs_cull_mode mode)
{
	switch (mode) {
	case GS_BACK:
		return D3D11_CULL_BACK;
	case GS_FRONT:
		return D3D11_CULL_FRONT;
	case GS_NEITHER:
		return D3D11_CULL_NONE;
	}

	return D3D11_CULL_BACK;
}

static inline D3D11_PRIMITIVE_TOPOLOGY ConvertGSTopology(gs_draw_mode mode)
{
	switch (mode) {
	case GS_POINTS:
		return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	case GS_LINES:
		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case GS_LINESTRIP:
		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case GS_TRIS:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case GS_TRISTRIP:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	}

	return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
}

/* exception-safe RAII wrapper for vertex buffer data (NOTE: not copy-safe) */
struct VBDataPtr {
	gs_vb_data *data;

	inline VBDataPtr(gs_vb_data *data) : data(data) {}
	inline ~VBDataPtr() { gs_vbdata_destroy(data); }
};

enum class gs_type {
	gs_vertex_buffer,
	gs_index_buffer,
	gs_texture_2d,
	gs_zstencil_buffer,
	gs_stage_surface,
	gs_sampler_state,
	gs_vertex_shader,
	gs_pixel_shader,
	gs_duplicator,
	gs_swap_chain,
	gs_timer,
	gs_timer_range,
	gs_texture_3d,
};

struct gs_obj {
	gs_device_t *device;
	gs_type obj_type;
	gs_obj *next;
	gs_obj **prev_next;

	inline gs_obj() : device(nullptr), next(nullptr), prev_next(nullptr) {}

	gs_obj(gs_device_t *device, gs_type type);
	virtual ~gs_obj();
};

struct gs_vertex_buffer : gs_obj {
	ComPtr<ID3D11Buffer> vertexBuffer;
	ComPtr<ID3D11Buffer> normalBuffer;
	ComPtr<ID3D11Buffer> colorBuffer;
	ComPtr<ID3D11Buffer> tangentBuffer;
	vector<ComPtr<ID3D11Buffer>> uvBuffers;

	bool dynamic;
	VBDataPtr vbd;
	size_t numVerts;
	vector<size_t> uvSizes;

	void FlushBuffer(ID3D11Buffer *buffer, void *array, size_t elementSize);

	void MakeBufferList(gs_vertex_shader *shader,
			    vector<ID3D11Buffer *> &buffers,
			    vector<uint32_t> &strides);

	void InitBuffer(const size_t elementSize, const size_t numVerts,
			void *array, ID3D11Buffer **buffer);

	void BuildBuffers();

	inline void Release()
	{
		vertexBuffer.Release();
		normalBuffer.Release();
		colorBuffer.Release();
		tangentBuffer.Release();
		uvBuffers.clear();
	}

	void Rebuild();

	gs_vertex_buffer(gs_device_t *device, struct gs_vb_data *data,
			 uint32_t flags);
};

/* exception-safe RAII wrapper for index buffer data (NOTE: not copy-safe) */
struct DataPtr {
	void *data;

	inline DataPtr(void *data) : data(data) {}
	inline ~DataPtr() { bfree(data); }
};

struct gs_index_buffer : gs_obj {
	ComPtr<ID3D11Buffer> indexBuffer;
	bool dynamic;
	gs_index_type type;
	size_t indexSize;
	size_t num;
	DataPtr indices;

	D3D11_BUFFER_DESC bd = {};
	D3D11_SUBRESOURCE_DATA srd = {};

	void InitBuffer();

	void Rebuild(ID3D11Device *dev);

	inline void Release() { indexBuffer.Release(); }

	gs_index_buffer(gs_device_t *device, enum gs_index_type type,
			void *indices, size_t num, uint32_t flags);
};

struct gs_timer : gs_obj {
	ComPtr<ID3D11Query> query_begin;
	ComPtr<ID3D11Query> query_end;

	void Rebuild(ID3D11Device *dev);

	inline void Release()
	{
		query_begin.Release();
		query_end.Release();
	}

	gs_timer(gs_device_t *device);
};

struct gs_timer_range : gs_obj {
	ComPtr<ID3D11Query> query_disjoint;

	void Rebuild(ID3D11Device *dev);

	inline void Release() { query_disjoint.Release(); }

	gs_timer_range(gs_device_t *device);
};

struct gs_texture : gs_obj {
	gs_texture_type type;
	uint32_t levels;
	gs_color_format format;

	ComPtr<ID3D11ShaderResourceView> shaderRes;
	ComPtr<ID3D11ShaderResourceView> shaderResLinear;
	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
	D3D11_SHADER_RESOURCE_VIEW_DESC viewDescLinear{};

	void Rebuild(ID3D11Device *dev);

	inline gs_texture(gs_texture_type type, uint32_t levels,
			  gs_color_format format)
		: type(type), levels(levels), format(format)
	{
	}

	inline gs_texture(gs_device *device, gs_type obj_type,
			  gs_texture_type type)
		: gs_obj(device, obj_type), type(type)
	{
	}

	inline gs_texture(gs_device *device, gs_type obj_type,
			  gs_texture_type type, uint32_t levels,
			  gs_color_format format)
		: gs_obj(device, obj_type),
		  type(type),
		  levels(levels),
		  format(format)
	{
	}
};

struct gs_texture_2d : gs_texture {
	ComPtr<ID3D11Texture2D> texture;
	ComPtr<ID3D11RenderTargetView> renderTarget[6];
	ComPtr<ID3D11RenderTargetView> renderTargetLinear[6];
	ComPtr<IDXGISurface1> gdiSurface;

	uint32_t width = 0, height = 0;
	uint32_t flags = 0;
	DXGI_FORMAT dxgiFormatResource = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT dxgiFormatView = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT dxgiFormatViewLinear = DXGI_FORMAT_UNKNOWN;
	bool isRenderTarget = false;
	bool isGDICompatible = false;
	bool isDynamic = false;
	bool isShared = false;
	bool genMipmaps = false;
	uint32_t sharedHandle = GS_INVALID_HANDLE;

	gs_texture_2d *pairedNV12texture = nullptr;
	bool nv12 = false;
	bool chroma = false;
	bool acquired = false;

	vector<vector<uint8_t>> data;
	vector<D3D11_SUBRESOURCE_DATA> srd;
	D3D11_TEXTURE2D_DESC td = {};

	void InitSRD(vector<D3D11_SUBRESOURCE_DATA> &srd);
	void InitTexture(const uint8_t *const *data);
	void InitResourceView();
	void InitRenderTargets();
	void BackupTexture(const uint8_t *const *data);
	void GetSharedHandle(IDXGIResource *dxgi_res);

	void RebuildSharedTextureFallback();
	void Rebuild(ID3D11Device *dev);
	void RebuildNV12_Y(ID3D11Device *dev);
	void RebuildNV12_UV(ID3D11Device *dev);

	inline void Release()
	{
		texture.Release();
		for (ComPtr<ID3D11RenderTargetView> &rt : renderTarget)
			rt.Release();
		for (ComPtr<ID3D11RenderTargetView> &rt : renderTargetLinear)
			rt.Release();
		gdiSurface.Release();
		shaderRes.Release();
		shaderResLinear.Release();
	}

	inline gs_texture_2d() : gs_texture(GS_TEXTURE_2D, 0, GS_UNKNOWN) {}

	gs_texture_2d(gs_device_t *device, uint32_t width, uint32_t height,
		      gs_color_format colorFormat, uint32_t levels,
		      const uint8_t *const *data, uint32_t flags,
		      gs_texture_type type, bool gdiCompatible,
		      bool nv12 = false);

	gs_texture_2d(gs_device_t *device, ID3D11Texture2D *nv12,
		      uint32_t flags);
	gs_texture_2d(gs_device_t *device, uint32_t handle);
	gs_texture_2d(gs_device_t *device, ID3D11Texture2D *obj);
};

struct gs_texture_3d : gs_texture {
	ComPtr<ID3D11Texture3D> texture;

	uint32_t width = 0, height = 0, depth = 0;
	uint32_t flags = 0;
	DXGI_FORMAT dxgiFormatResource = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT dxgiFormatView = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT dxgiFormatViewLinear = DXGI_FORMAT_UNKNOWN;
	bool isDynamic = false;
	bool isShared = false;
	bool genMipmaps = false;
	uint32_t sharedHandle = GS_INVALID_HANDLE;

	bool chroma = false;
	bool acquired = false;

	vector<vector<uint8_t>> data;
	vector<D3D11_SUBRESOURCE_DATA> srd;
	D3D11_TEXTURE3D_DESC td = {};

	void InitSRD(vector<D3D11_SUBRESOURCE_DATA> &srd);
	void InitTexture(const uint8_t *const *data);
	void InitResourceView();
	void BackupTexture(const uint8_t *const *data);
	void GetSharedHandle(IDXGIResource *dxgi_res);

	void RebuildSharedTextureFallback();
	void Rebuild(ID3D11Device *dev);
	void RebuildNV12_Y(ID3D11Device *dev);
	void RebuildNV12_UV(ID3D11Device *dev);

	inline void Release()
	{
		texture.Release();
		shaderRes.Release();
	}

	inline gs_texture_3d() : gs_texture(GS_TEXTURE_3D, 0, GS_UNKNOWN) {}

	gs_texture_3d(gs_device_t *device, uint32_t width, uint32_t height,
		      uint32_t depth, gs_color_format colorFormat,
		      uint32_t levels, const uint8_t *const *data,
		      uint32_t flags);

	gs_texture_3d(gs_device_t *device, uint32_t handle);
};

struct gs_zstencil_buffer : gs_obj {
	ComPtr<ID3D11Texture2D> texture;
	ComPtr<ID3D11DepthStencilView> view;

	uint32_t width, height;
	gs_zstencil_format format;
	DXGI_FORMAT dxgiFormat;

	D3D11_TEXTURE2D_DESC td = {};
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};

	void InitBuffer();

	void Rebuild(ID3D11Device *dev);

	inline void Release()
	{
		texture.Release();
		view.Release();
	}

	inline gs_zstencil_buffer()
		: width(0), height(0), dxgiFormat(DXGI_FORMAT_UNKNOWN)
	{
	}

	gs_zstencil_buffer(gs_device_t *device, uint32_t width, uint32_t height,
			   gs_zstencil_format format);
};

struct gs_stage_surface : gs_obj {
	ComPtr<ID3D11Texture2D> texture;
	D3D11_TEXTURE2D_DESC td = {};

	uint32_t width, height;
	gs_color_format format;
	DXGI_FORMAT dxgiFormat;

	void Rebuild(ID3D11Device *dev);

	inline void Release() { texture.Release(); }

	gs_stage_surface(gs_device_t *device, uint32_t width, uint32_t height,
			 gs_color_format colorFormat);
	gs_stage_surface(gs_device_t *device, uint32_t width, uint32_t height);
};

struct gs_sampler_state : gs_obj {
	ComPtr<ID3D11SamplerState> state;
	D3D11_SAMPLER_DESC sd = {};
	gs_sampler_info info;

	void Rebuild(ID3D11Device *dev);

	inline void Release() { state.Release(); }

	gs_sampler_state(gs_device_t *device, const gs_sampler_info *info);
};

struct gs_shader_param {
	string name;
	gs_shader_param_type type;

	uint32_t textureID;
	struct gs_sampler_state *nextSampler = nullptr;

	int arrayCount;

	size_t pos;

	vector<uint8_t> curValue;
	vector<uint8_t> defaultValue;
	bool changed;

	gs_shader_param(shader_var &var, uint32_t &texCounter);
};

struct ShaderError {
	ComPtr<ID3D10Blob> errors;
	HRESULT hr;

	inline ShaderError(const ComPtr<ID3D10Blob> &errors, HRESULT hr)
		: errors(errors), hr(hr)
	{
	}
};

struct gs_shader : gs_obj {
	gs_shader_type type;
	vector<gs_shader_param> params;
	ComPtr<ID3D11Buffer> constants;
	size_t constantSize;

	D3D11_BUFFER_DESC bd = {};
	vector<uint8_t> data;

	inline void UpdateParam(vector<uint8_t> &constData,
				gs_shader_param &param, bool &upload);
	void UploadParams();

	void BuildConstantBuffer();
	void Compile(const char *shaderStr, const char *file,
		     const char *target, ID3D10Blob **shader);

	inline gs_shader(gs_device_t *device, gs_type obj_type,
			 gs_shader_type type)
		: gs_obj(device, obj_type), type(type), constantSize(0)
	{
	}

	virtual ~gs_shader() {}
};

struct ShaderSampler {
	string name;
	gs_sampler_state sampler;

	inline ShaderSampler(const char *name, gs_device_t *device,
			     gs_sampler_info *info)
		: name(name), sampler(device, info)
	{
	}
};

struct gs_vertex_shader : gs_shader {
	ComPtr<ID3D11VertexShader> shader;
	ComPtr<ID3D11InputLayout> layout;

	gs_shader_param *world, *viewProj;

	vector<D3D11_INPUT_ELEMENT_DESC> layoutData;

	bool hasNormals;
	bool hasColors;
	bool hasTangents;
	uint32_t nTexUnits;

	void Rebuild(ID3D11Device *dev);

	inline void Release()
	{
		shader.Release();
		layout.Release();
		constants.Release();
	}

	inline uint32_t NumBuffersExpected() const
	{
		uint32_t count = nTexUnits + 1;
		if (hasNormals)
			count++;
		if (hasColors)
			count++;
		if (hasTangents)
			count++;

		return count;
	}

	void GetBuffersExpected(const vector<D3D11_INPUT_ELEMENT_DESC> &inputs);

	gs_vertex_shader(gs_device_t *device, const char *file,
			 const char *shaderString);
};

struct gs_duplicator : gs_obj {
	ComPtr<IDXGIOutputDuplication> duplicator;
	gs_texture_2d *texture;
	int idx;
	long refs;
	bool updated;

	void Start();

	inline void Release() { duplicator.Release(); }

	gs_duplicator(gs_device_t *device, int monitor_idx);
	~gs_duplicator();
};

struct gs_pixel_shader : gs_shader {
	ComPtr<ID3D11PixelShader> shader;
	vector<unique_ptr<ShaderSampler>> samplers;

	void Rebuild(ID3D11Device *dev);

	inline void Release()
	{
		shader.Release();
		constants.Release();
	}

	inline void GetSamplerStates(ID3D11SamplerState **states)
	{
		size_t i;
		for (i = 0; i < samplers.size(); i++)
			states[i] = samplers[i]->sampler.state;
		for (; i < GS_MAX_TEXTURES; i++)
			states[i] = NULL;
	}

	gs_pixel_shader(gs_device_t *device, const char *file,
			const char *shaderString);
};

struct gs_swap_chain : gs_obj {
	uint32_t numBuffers;
	HWND hwnd;
	gs_init_data initData;
	DXGI_SWAP_CHAIN_DESC swapDesc = {};

	gs_texture_2d target;
	gs_zstencil_buffer zs;
	ComPtr<IDXGISwapChain> swap;

	void InitTarget(uint32_t cx, uint32_t cy);
	void InitZStencilBuffer(uint32_t cx, uint32_t cy);
	void Resize(uint32_t cx, uint32_t cy);
	void Init();

	void Rebuild(ID3D11Device *dev);

	inline void Release()
	{
		target.Release();
		zs.Release();
		swap.Release();
	}

	gs_swap_chain(gs_device *device, const gs_init_data *data);
};

struct BlendState {
	bool blendEnabled;
	gs_blend_type srcFactorC;
	gs_blend_type destFactorC;
	gs_blend_type srcFactorA;
	gs_blend_type destFactorA;

	bool redEnabled;
	bool greenEnabled;
	bool blueEnabled;
	bool alphaEnabled;

	inline BlendState()
		: blendEnabled(true),
		  srcFactorC(GS_BLEND_SRCALPHA),
		  destFactorC(GS_BLEND_INVSRCALPHA),
		  srcFactorA(GS_BLEND_ONE),
		  destFactorA(GS_BLEND_INVSRCALPHA),
		  redEnabled(true),
		  greenEnabled(true),
		  blueEnabled(true),
		  alphaEnabled(true)
	{
	}

	inline BlendState(const BlendState &state)
	{
		memcpy(this, &state, sizeof(BlendState));
	}
};

struct SavedBlendState : BlendState {
	ComPtr<ID3D11BlendState> state;
	D3D11_BLEND_DESC bd;

	void Rebuild(ID3D11Device *dev);

	inline void Release() { state.Release(); }

	inline SavedBlendState(const BlendState &val, D3D11_BLEND_DESC &desc)
		: BlendState(val), bd(desc)
	{
	}
};

struct StencilSide {
	gs_depth_test test;
	gs_stencil_op_type fail;
	gs_stencil_op_type zfail;
	gs_stencil_op_type zpass;

	inline StencilSide()
		: test(GS_ALWAYS), fail(GS_KEEP), zfail(GS_KEEP), zpass(GS_KEEP)
	{
	}
};

struct ZStencilState {
	bool depthEnabled;
	bool depthWriteEnabled;
	gs_depth_test depthFunc;

	bool stencilEnabled;
	bool stencilWriteEnabled;
	StencilSide stencilFront;
	StencilSide stencilBack;

	inline ZStencilState()
		: depthEnabled(true),
		  depthWriteEnabled(true),
		  depthFunc(GS_LESS),
		  stencilEnabled(false),
		  stencilWriteEnabled(true)
	{
	}

	inline ZStencilState(const ZStencilState &state)
	{
		memcpy(this, &state, sizeof(ZStencilState));
	}
};

struct SavedZStencilState : ZStencilState {
	ComPtr<ID3D11DepthStencilState> state;
	D3D11_DEPTH_STENCIL_DESC dsd;

	void Rebuild(ID3D11Device *dev);

	inline void Release() { state.Release(); }

	inline SavedZStencilState(const ZStencilState &val,
				  D3D11_DEPTH_STENCIL_DESC desc)
		: ZStencilState(val), dsd(desc)
	{
	}
};

struct RasterState {
	gs_cull_mode cullMode;
	bool scissorEnabled;

	inline RasterState() : cullMode(GS_BACK), scissorEnabled(false) {}

	inline RasterState(const RasterState &state)
	{
		memcpy(this, &state, sizeof(RasterState));
	}
};

struct SavedRasterState : RasterState {
	ComPtr<ID3D11RasterizerState> state;
	D3D11_RASTERIZER_DESC rd;

	void Rebuild(ID3D11Device *dev);

	inline void Release() { state.Release(); }

	inline SavedRasterState(const RasterState &val,
				D3D11_RASTERIZER_DESC &desc)
		: RasterState(val), rd(desc)
	{
	}
};

struct mat4float {
	float mat[16];
};

struct gs_device {
	ComPtr<IDXGIFactory1> factory;
	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	uint32_t adpIdx = 0;
	bool nv12Supported = false;

	gs_texture_2d *curRenderTarget = nullptr;
	gs_zstencil_buffer *curZStencilBuffer = nullptr;
	int curRenderSide = 0;
	bool curFramebufferSrgb = false;
	bool curFramebufferInvalidate = false;
	gs_texture *curTextures[GS_MAX_TEXTURES];
	gs_sampler_state *curSamplers[GS_MAX_TEXTURES];
	gs_vertex_buffer *curVertexBuffer = nullptr;
	gs_index_buffer *curIndexBuffer = nullptr;
	gs_vertex_shader *curVertexShader = nullptr;
	gs_pixel_shader *curPixelShader = nullptr;
	gs_swap_chain *curSwapChain = nullptr;

	gs_vertex_buffer *lastVertexBuffer = nullptr;
	gs_vertex_shader *lastVertexShader = nullptr;

	bool zstencilStateChanged = true;
	bool rasterStateChanged = true;
	bool blendStateChanged = true;
	ZStencilState zstencilState;
	RasterState rasterState;
	BlendState blendState;
	vector<SavedZStencilState> zstencilStates;
	vector<SavedRasterState> rasterStates;
	vector<SavedBlendState> blendStates;
	ID3D11DepthStencilState *curDepthStencilState = nullptr;
	ID3D11RasterizerState *curRasterState = nullptr;
	ID3D11BlendState *curBlendState = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY curToplogy;

	pD3DCompile d3dCompile = nullptr;
#ifdef DISASSEMBLE_SHADERS
	pD3DDisassemble d3dDisassemble = nullptr;
#endif

	gs_rect viewport;

	vector<mat4float> projStack;

	matrix4 curProjMatrix;
	matrix4 curViewMatrix;
	matrix4 curViewProjMatrix;

	vector<gs_device_loss> loss_callbacks;
	gs_obj *first_obj = nullptr;

	void InitCompiler();
	void InitFactory(uint32_t adapterIdx);
	void InitDevice(uint32_t adapterIdx);

	ID3D11DepthStencilState *AddZStencilState();
	ID3D11RasterizerState *AddRasterState();
	ID3D11BlendState *AddBlendState();
	void UpdateZStencilState();
	void UpdateRasterState();
	void UpdateBlendState();

	void LoadVertexBufferData();

	inline void CopyTex(ID3D11Texture2D *dst, uint32_t dst_x,
			    uint32_t dst_y, gs_texture_t *src, uint32_t src_x,
			    uint32_t src_y, uint32_t src_w, uint32_t src_h);

	void UpdateViewProjMatrix();

	void FlushOutputViews();

	void RebuildDevice();

	bool HasBadNV12Output();

	gs_device(uint32_t adapterIdx);
	~gs_device();
};

extern "C" EXPORT int device_texture_acquire_sync(gs_texture_t *tex,
						  uint64_t key, uint32_t ms);
