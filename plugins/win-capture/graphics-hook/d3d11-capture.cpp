#include <d3d11.h>
#include <dxgi.h>

#include "dxgi-helpers.hpp"
#include "graphics-hook.h"
#include "../funchook.h"

struct d3d11_data {
	ID3D11Device *device;         /* do not release */
	ID3D11DeviceContext *context; /* do not release */
	uint32_t cx;
	uint32_t cy;
	DXGI_FORMAT format;
	bool using_shtex;
	bool multisampled;

	ID3D11Texture2D *scale_tex;
	ID3D11ShaderResourceView *scale_resource;

	ID3D11VertexShader *vertex_shader;
	ID3D11InputLayout *vertex_layout;
	ID3D11PixelShader *pixel_shader;

	ID3D11SamplerState *sampler_state;
	ID3D11BlendState *blend_state;
	ID3D11DepthStencilState *zstencil_state;
	ID3D11RasterizerState *raster_state;

	ID3D11Buffer *vertex_buffer;

	union {
		/* shared texture */
		struct {
			struct shtex_data *shtex_info;
			ID3D11Texture2D *texture;
			HANDLE handle;
		};
		/* shared memory */
		struct {
			ID3D11Texture2D *copy_surfaces[NUM_BUFFERS];
			bool texture_ready[NUM_BUFFERS];
			bool texture_mapped[NUM_BUFFERS];
			uint32_t pitch;
			struct shmem_data *shmem_info;
			int cur_tex;
			int copy_wait;
		};
	};
};

static struct d3d11_data data = {};

void d3d11_free(void)
{
	if (data.scale_tex)
		data.scale_tex->Release();
	if (data.scale_resource)
		data.scale_resource->Release();
	if (data.vertex_shader)
		data.vertex_shader->Release();
	if (data.vertex_layout)
		data.vertex_layout->Release();
	if (data.pixel_shader)
		data.pixel_shader->Release();
	if (data.sampler_state)
		data.sampler_state->Release();
	if (data.blend_state)
		data.blend_state->Release();
	if (data.zstencil_state)
		data.zstencil_state->Release();
	if (data.raster_state)
		data.raster_state->Release();
	if (data.vertex_buffer)
		data.vertex_buffer->Release();

	capture_free();

	if (data.using_shtex) {
		if (data.texture)
			data.texture->Release();
	} else {
		for (size_t i = 0; i < NUM_BUFFERS; i++) {
			if (data.copy_surfaces[i]) {
				if (data.texture_mapped[i])
					data.context->Unmap(
						data.copy_surfaces[i], 0);
				data.copy_surfaces[i]->Release();
			}
		}
	}

	memset(&data, 0, sizeof(data));

	hlog("----------------- d3d11 capture freed ----------------");
}

static bool create_d3d11_stage_surface(ID3D11Texture2D **tex)
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = data.cx;
	desc.Height = data.cy;
	desc.Format = data.format;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	hr = data.device->CreateTexture2D(&desc, nullptr, tex);
	if (FAILED(hr)) {
		hlog_hr("create_d3d11_stage_surface: failed to create texture",
			hr);
		return false;
	}

	return true;
}

static bool create_d3d11_tex(uint32_t cx, uint32_t cy, ID3D11Texture2D **tex,
			     HANDLE *handle)
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = cx;
	desc.Height = cy;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = apply_dxgi_format_typeless(
		data.format, global_hook_info->allow_srgb_alias);
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	hr = data.device->CreateTexture2D(&desc, nullptr, tex);
	if (FAILED(hr)) {
		hlog_hr("create_d3d11_tex: failed to create texture", hr);
		return false;
	}

	if (!!handle) {
		IDXGIResource *dxgi_res;
		hr = (*tex)->QueryInterface(__uuidof(IDXGIResource),
					    (void **)&dxgi_res);
		if (FAILED(hr)) {
			hlog_hr("create_d3d11_tex: failed to query "
				"IDXGIResource interface from texture",
				hr);
			return false;
		}

		hr = dxgi_res->GetSharedHandle(handle);
		dxgi_res->Release();
		if (FAILED(hr)) {
			hlog_hr("create_d3d11_tex: failed to get shared handle",
				hr);
			return false;
		}
	}

	return true;
}

static inline bool d3d11_init_format(IDXGISwapChain *swap, HWND &window)
{
	DXGI_SWAP_CHAIN_DESC desc;
	HRESULT hr;

	hr = swap->GetDesc(&desc);
	if (FAILED(hr)) {
		hlog_hr("d3d11_init_format: swap->GetDesc failed", hr);
		return false;
	}

	data.format = strip_dxgi_format_srgb(desc.BufferDesc.Format);
	data.multisampled = desc.SampleDesc.Count > 1;
	window = desc.OutputWindow;
	data.cx = desc.BufferDesc.Width;
	data.cy = desc.BufferDesc.Height;

	return true;
}

static bool d3d11_shmem_init_buffers(size_t idx)
{
	bool success;

	success = create_d3d11_stage_surface(&data.copy_surfaces[idx]);
	if (!success) {
		hlog("d3d11_shmem_init_buffers: failed to create copy surface");
		return false;
	}

	if (idx == 0) {
		D3D11_MAPPED_SUBRESOURCE map = {};
		HRESULT hr;

		hr = data.context->Map(data.copy_surfaces[idx], 0,
				       D3D11_MAP_READ, 0, &map);
		if (FAILED(hr)) {
			hlog_hr("d3d11_shmem_init_buffers: failed to get "
				"pitch",
				hr);
			return false;
		}

		data.pitch = map.RowPitch;
		data.context->Unmap(data.copy_surfaces[idx], 0);
	}

	return true;
}

static bool d3d11_shmem_init(HWND window)
{
	data.using_shtex = false;

	for (size_t i = 0; i < NUM_BUFFERS; i++) {
		if (!d3d11_shmem_init_buffers(i)) {
			return false;
		}
	}
	if (!capture_init_shmem(&data.shmem_info, window, data.cx, data.cy,
				data.pitch, data.format, false)) {
		return false;
	}

	hlog("d3d11 memory capture successful");
	return true;
}

static bool d3d11_shtex_init(HWND window)
{
	bool success;

	data.using_shtex = true;

	success =
		create_d3d11_tex(data.cx, data.cy, &data.texture, &data.handle);

	if (!success) {
		hlog("d3d11_shtex_init: failed to create texture");
		return false;
	}
	if (!capture_init_shtex(&data.shtex_info, window, data.cx, data.cy,
				data.format, false, (uintptr_t)data.handle)) {
		return false;
	}

	hlog("d3d11 shared texture capture successful");
	return true;
}

static void d3d11_init(IDXGISwapChain *swap)
{
	HWND window;
	HRESULT hr;

	hr = swap->GetDevice(__uuidof(ID3D11Device), (void **)&data.device);
	if (FAILED(hr)) {
		hlog_hr("d3d11_init: failed to get device from swap", hr);
		return;
	}

	data.device->Release();

	data.device->GetImmediateContext(&data.context);
	data.context->Release();

	if (!d3d11_init_format(swap, window)) {
		return;
	}

	const bool success = global_hook_info->force_shmem
				     ? d3d11_shmem_init(window)
				     : d3d11_shtex_init(window);
	if (!success)
		d3d11_free();
}

static inline void d3d11_copy_texture(ID3D11Resource *dst, ID3D11Resource *src)
{
	if (data.multisampled) {
		data.context->ResolveSubresource(dst, 0, src, 0, data.format);
	} else {
		data.context->CopyResource(dst, src);
	}
}

static inline void d3d11_shtex_capture(ID3D11Resource *backbuffer)
{
	d3d11_copy_texture(data.texture, backbuffer);
}

static void d3d11_shmem_capture_copy(int i)
{
	D3D11_MAPPED_SUBRESOURCE map;
	HRESULT hr;

	if (data.texture_ready[i]) {
		data.texture_ready[i] = false;

		hr = data.context->Map(data.copy_surfaces[i], 0, D3D11_MAP_READ,
				       0, &map);
		if (SUCCEEDED(hr)) {
			data.texture_mapped[i] = true;
			shmem_copy_data(i, map.pData);
		}
	}
}

static inline void d3d11_shmem_capture(ID3D11Resource *backbuffer)
{
	int next_tex;

	next_tex = (data.cur_tex + 1) % NUM_BUFFERS;
	d3d11_shmem_capture_copy(next_tex);

	if (data.copy_wait < NUM_BUFFERS - 1) {
		data.copy_wait++;
	} else {
		if (shmem_texture_data_lock(data.cur_tex)) {
			data.context->Unmap(data.copy_surfaces[data.cur_tex],
					    0);
			data.texture_mapped[data.cur_tex] = false;
			shmem_texture_data_unlock(data.cur_tex);
		}

		d3d11_copy_texture(data.copy_surfaces[data.cur_tex],
				   backbuffer);
		data.texture_ready[data.cur_tex] = true;
	}

	data.cur_tex = next_tex;
}

void d3d11_capture(void *swap_ptr, void *backbuffer_ptr, bool)
{
	IDXGIResource *dxgi_backbuffer = (IDXGIResource *)backbuffer_ptr;
	IDXGISwapChain *swap = (IDXGISwapChain *)swap_ptr;

	HRESULT hr;
	if (capture_should_stop()) {
		d3d11_free();
	}
	if (capture_should_init()) {
		d3d11_init(swap);
	}
	if (capture_ready()) {
		ID3D11Resource *backbuffer;

		hr = dxgi_backbuffer->QueryInterface(__uuidof(ID3D11Resource),
						     (void **)&backbuffer);
		if (FAILED(hr)) {
			hlog_hr("d3d11_shtex_capture: failed to get "
				"backbuffer",
				hr);
			return;
		}

		if (data.using_shtex)
			d3d11_shtex_capture(backbuffer);
		else
			d3d11_shmem_capture(backbuffer);

		backbuffer->Release();
	}
}
