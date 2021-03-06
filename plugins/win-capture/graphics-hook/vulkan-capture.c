#include <windows.h>
#include "graphics-hook.h"

#define VK_USE_PLATFORM_WIN32_KHR

#include <malloc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

#include <vulkan/vulkan_win32.h>

#define COBJMACROS
#include <dxgi.h>
#include <d3d11.h>

#include "dxgi-helpers.hpp"

#include "vulkan-capture.h"

/* ======================================================================== */
/* defs/statics                                                             */

/* use the loader's dispatch table pointer as a key for internal data maps */
#define GET_LDT(x) (*(void **)x)

static bool vulkan_seen = false;

/* ======================================================================== */
/* hook data                                                                */

struct vk_obj_node {
	uint64_t obj;
	struct vk_obj_node *next;
};

struct vk_obj_list {
	struct vk_obj_node *root;
	SRWLOCK mutex;
};

struct vk_swap_data {
	struct vk_obj_node node;

	VkExtent2D image_extent;
	VkFormat format;
	HWND hwnd;
	VkImage export_image;
	bool layout_initialized;
	VkDeviceMemory export_mem;
	VkImage *swap_images;
	uint32_t image_count;

	HANDLE handle;
	struct shtex_data *shtex_info;
	ID3D11Texture2D *d3d11_tex;
	bool captured;
};

struct vk_queue_data {
	struct vk_obj_node node;

	uint32_t fam_idx;
	bool supports_transfer;
	struct vk_frame_data *frames;
	uint32_t frame_index;
	uint32_t frame_count;
};

struct vk_frame_data {
	VkCommandPool cmd_pool;
	VkCommandBuffer cmd_buffer;
	VkFence fence;
	bool cmd_buffer_busy;
};

struct vk_surf_data {
	struct vk_obj_node node;

	HWND hwnd;
};

struct vk_inst_data {
	struct vk_obj_node node;

	VkInstance instance;

	bool valid;

	struct vk_inst_funcs funcs;
	struct vk_obj_list surfaces;
};

struct vk_data {
	struct vk_obj_node node;

	VkDevice device;

	bool valid;

	struct vk_device_funcs funcs;
	VkPhysicalDevice phy_device;
	struct vk_obj_list swaps;
	struct vk_swap_data *cur_swap;

	struct vk_obj_list queues;

	VkExternalMemoryProperties external_mem_props;

	struct vk_inst_data *inst_data;

	VkAllocationCallbacks ac_storage;
	const VkAllocationCallbacks *ac;

	ID3D11Device *d3d11_device;
	ID3D11DeviceContext *d3d11_context;
};

/* ------------------------------------------------------------------------- */

static void *vk_alloc(const VkAllocationCallbacks *ac, size_t size,
		      size_t alignment, enum VkSystemAllocationScope scope)
{
	return ac ? ac->pfnAllocation(ac->pUserData, size, alignment, scope)
		  : _aligned_malloc(size, alignment);
}

static void vk_free(const VkAllocationCallbacks *ac, void *memory)
{
	if (ac)
		ac->pfnFree(ac->pUserData, memory);
	else
		_aligned_free(memory);
}

static void add_obj_data(struct vk_obj_list *list, uint64_t obj, void *data)
{
	AcquireSRWLockExclusive(&list->mutex);

	struct vk_obj_node *const node = data;
	node->obj = obj;
	node->next = list->root;
	list->root = node;

	ReleaseSRWLockExclusive(&list->mutex);
}

static struct vk_obj_node *get_obj_data(struct vk_obj_list *list, uint64_t obj)
{
	struct vk_obj_node *data = NULL;

	AcquireSRWLockExclusive(&list->mutex);

	struct vk_obj_node *node = list->root;
	while (node) {
		if (node->obj == obj) {
			data = node;
			break;
		}

		node = node->next;
	}

	ReleaseSRWLockExclusive(&list->mutex);

	return data;
}

static struct vk_obj_node *remove_obj_data(struct vk_obj_list *list,
					   uint64_t obj)
{
	struct vk_obj_node *data = NULL;

	AcquireSRWLockExclusive(&list->mutex);

	struct vk_obj_node *prev = NULL;
	struct vk_obj_node *node = list->root;
	while (node) {
		if (node->obj == obj) {
			data = node;
			if (prev)
				prev->next = node->next;
			else
				list->root = node->next;
			break;
		}

		prev = node;
		node = node->next;
	}

	ReleaseSRWLockExclusive(&list->mutex);

	return data;
}

static void init_obj_list(struct vk_obj_list *list)
{
	list->root = NULL;
	InitializeSRWLock(&list->mutex);
}

static struct vk_obj_node *obj_walk_begin(struct vk_obj_list *list)
{
	AcquireSRWLockExclusive(&list->mutex);
	return list->root;
}

static struct vk_obj_node *obj_walk_next(struct vk_obj_node *node)
{
	return node->next;
}

static void obj_walk_end(struct vk_obj_list *list)
{
	ReleaseSRWLockExclusive(&list->mutex);
}

/* ------------------------------------------------------------------------- */

static struct vk_obj_list devices;

static struct vk_data *alloc_device_data(const VkAllocationCallbacks *ac)
{
	struct vk_data *data = vk_alloc(ac, sizeof(struct vk_data),
					_Alignof(struct vk_data),
					VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
	return data;
}

static void init_device_data(struct vk_data *data, VkDevice device)
{
	add_obj_data(&devices, (uint64_t)GET_LDT(device), data);
	data->device = device;
}

static struct vk_data *get_device_data(VkDevice device)
{
	return (struct vk_data *)get_obj_data(&devices,
					      (uint64_t)GET_LDT(device));
}

static struct vk_data *get_device_data_by_queue(VkQueue queue)
{
	return (struct vk_data *)get_obj_data(&devices,
					      (uint64_t)GET_LDT(queue));
}

static struct vk_data *remove_device_data(VkDevice device)
{
	return (struct vk_data *)remove_obj_data(&devices,
						 (uint64_t)GET_LDT(device));
}

static void free_device_data(struct vk_data *data,
			     const VkAllocationCallbacks *ac)
{
	vk_free(ac, data);
}

/* ------------------------------------------------------------------------- */

static struct vk_queue_data *add_queue_data(struct vk_data *data, VkQueue queue,
					    uint32_t fam_idx,
					    bool supports_transfer,
					    const VkAllocationCallbacks *ac)
{
	struct vk_queue_data *const queue_data =
		vk_alloc(ac, sizeof(struct vk_queue_data),
			 _Alignof(struct vk_queue_data),
			 VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
	add_obj_data(&data->queues, (uint64_t)queue, queue_data);
	queue_data->fam_idx = fam_idx;
	queue_data->supports_transfer = supports_transfer;
	queue_data->frames = NULL;
	queue_data->frame_index = 0;
	queue_data->frame_count = 0;
	return queue_data;
}

static struct vk_queue_data *get_queue_data(struct vk_data *data, VkQueue queue)
{
	return (struct vk_queue_data *)get_obj_data(&data->queues,
						    (uint64_t)queue);
}

static VkQueue get_queue_key(const struct vk_queue_data *queue_data)
{
	return (VkQueue)(uintptr_t)queue_data->node.obj;
}

static void remove_free_queue_all(struct vk_data *data,
				  const VkAllocationCallbacks *ac)
{
	struct vk_queue_data *queue_data =
		(struct vk_queue_data *)data->queues.root;
	while (data->queues.root) {
		remove_obj_data(&data->queues, queue_data->node.obj);
		vk_free(ac, queue_data);

		queue_data = (struct vk_queue_data *)data->queues.root;
	}
}

static struct vk_queue_data *queue_walk_begin(struct vk_data *data)
{
	return (struct vk_queue_data *)obj_walk_begin(&data->queues);
}

static struct vk_queue_data *queue_walk_next(struct vk_queue_data *queue_data)
{
	return (struct vk_queue_data *)obj_walk_next(
		(struct vk_obj_node *)queue_data);
}

static void queue_walk_end(struct vk_data *data)
{
	obj_walk_end(&data->queues);
}

/* ------------------------------------------------------------------------- */

static struct vk_swap_data *alloc_swap_data(const VkAllocationCallbacks *ac)
{
	struct vk_swap_data *const swap_data = vk_alloc(
		ac, sizeof(struct vk_swap_data), _Alignof(struct vk_swap_data),
		VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
	return swap_data;
}

static void init_swap_data(struct vk_swap_data *swap_data, struct vk_data *data,
			   VkSwapchainKHR sc)
{
	add_obj_data(&data->swaps, (uint64_t)sc, swap_data);
}

static struct vk_swap_data *get_swap_data(struct vk_data *data,
					  VkSwapchainKHR sc)
{
	return (struct vk_swap_data *)get_obj_data(&data->swaps, (uint64_t)sc);
}

static void remove_free_swap_data(struct vk_data *data, VkSwapchainKHR sc,
				  const VkAllocationCallbacks *ac)
{
	struct vk_swap_data *const swap_data =
		(struct vk_swap_data *)remove_obj_data(&data->swaps,
						       (uint64_t)sc);
	vk_free(ac, swap_data);
}

static struct vk_swap_data *swap_walk_begin(struct vk_data *data)
{
	return (struct vk_swap_data *)obj_walk_begin(&data->swaps);
}

static struct vk_swap_data *swap_walk_next(struct vk_swap_data *swap_data)
{
	return (struct vk_swap_data *)obj_walk_next(
		(struct vk_obj_node *)swap_data);
}

static void swap_walk_end(struct vk_data *data)
{
	obj_walk_end(&data->swaps);
}

/* ------------------------------------------------------------------------- */

static void vk_shtex_clear_fence(const struct vk_data *data,
				 struct vk_frame_data *frame_data)
{
	const VkFence fence = frame_data->fence;
	if (frame_data->cmd_buffer_busy) {
		VkDevice device = data->device;
		const struct vk_device_funcs *funcs = &data->funcs;
		funcs->WaitForFences(device, 1, &fence, VK_TRUE, ~0ull);
		funcs->ResetFences(device, 1, &fence);
		frame_data->cmd_buffer_busy = false;
	}
}

static void vk_shtex_wait_until_pool_idle(struct vk_data *data,
					  struct vk_queue_data *queue_data)
{
	for (uint32_t frame_idx = 0; frame_idx < queue_data->frame_count;
	     frame_idx++) {
		struct vk_frame_data *frame_data =
			&queue_data->frames[frame_idx];
		if (frame_data->cmd_pool != VK_NULL_HANDLE)
			vk_shtex_clear_fence(data, frame_data);
	}
}

static void vk_shtex_wait_until_idle(struct vk_data *data)
{
	struct vk_queue_data *queue_data = queue_walk_begin(data);

	while (queue_data) {
		vk_shtex_wait_until_pool_idle(data, queue_data);

		queue_data = queue_walk_next(queue_data);
	}

	queue_walk_end(data);
}

static void vk_shtex_free(struct vk_data *data)
{
	capture_free();

	vk_shtex_wait_until_idle(data);

	struct vk_swap_data *swap = swap_walk_begin(data);

	while (swap) {
		VkDevice device = data->device;
		if (swap->export_image)
			data->funcs.DestroyImage(device, swap->export_image,
						 data->ac);

		if (swap->export_mem)
			data->funcs.FreeMemory(device, swap->export_mem, NULL);

		if (swap->d3d11_tex) {
			ID3D11Texture2D_Release(swap->d3d11_tex);
		}

		swap->handle = INVALID_HANDLE_VALUE;
		swap->d3d11_tex = NULL;
		swap->export_mem = VK_NULL_HANDLE;
		swap->export_image = VK_NULL_HANDLE;

		swap->captured = false;

		swap = swap_walk_next(swap);
	}

	swap_walk_end(data);

	if (data->d3d11_context) {
		ID3D11DeviceContext_Release(data->d3d11_context);
		data->d3d11_context = NULL;
	}
	if (data->d3d11_device) {
		ID3D11Device_Release(data->d3d11_device);
		data->d3d11_device = NULL;
	}

	data->cur_swap = NULL;

	hlog("------------------ vulkan capture freed ------------------");
}

/* ------------------------------------------------------------------------- */

static void add_surf_data(struct vk_inst_data *idata, VkSurfaceKHR surf,
			  HWND hwnd, const VkAllocationCallbacks *ac)
{
	struct vk_surf_data *surf_data = vk_alloc(
		ac, sizeof(struct vk_surf_data), _Alignof(struct vk_surf_data),
		VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
	if (surf_data) {
		surf_data->hwnd = hwnd;

		add_obj_data(&idata->surfaces, (uint64_t)surf, surf_data);
	}
}

static HWND find_surf_hwnd(struct vk_inst_data *idata, VkSurfaceKHR surf)
{
	struct vk_surf_data *surf_data = (struct vk_surf_data *)get_obj_data(
		&idata->surfaces, (uint64_t)surf);
	return surf_data->hwnd;
}

static void remove_free_surf_data(struct vk_inst_data *idata, VkSurfaceKHR surf,
				  const VkAllocationCallbacks *ac)
{
	struct vk_surf_data *surf_data = (struct vk_surf_data *)remove_obj_data(
		&idata->surfaces, (uint64_t)surf);
	vk_free(ac, surf_data);
}

/* ------------------------------------------------------------------------- */

static struct vk_obj_list instances;

static struct vk_inst_data *alloc_inst_data(const VkAllocationCallbacks *ac)
{
	struct vk_inst_data *idata = vk_alloc(
		ac, sizeof(struct vk_inst_data), _Alignof(struct vk_inst_data),
		VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
	return idata;
}

static void init_inst_data(struct vk_inst_data *idata, VkInstance instance)
{
	add_obj_data(&instances, (uint64_t)GET_LDT(instance), idata);
	idata->instance = instance;
}

static struct vk_inst_data *get_inst_data(VkInstance instance)
{
	return (struct vk_inst_data *)get_obj_data(&instances,
						   (uint64_t)GET_LDT(instance));
}

static struct vk_inst_funcs *get_inst_funcs(VkInstance instance)
{
	struct vk_inst_data *idata =
		(struct vk_inst_data *)get_inst_data(instance);
	return &idata->funcs;
}

static struct vk_inst_data *
get_inst_data_by_physical_device(VkPhysicalDevice physicalDevice)
{
	return (struct vk_inst_data *)get_obj_data(
		&instances, (uint64_t)GET_LDT(physicalDevice));
}

static struct vk_inst_funcs *
get_inst_funcs_by_physical_device(VkPhysicalDevice physicalDevice)
{
	struct vk_inst_data *idata =
		(struct vk_inst_data *)get_inst_data_by_physical_device(
			physicalDevice);
	return &idata->funcs;
}

static void remove_free_inst_data(VkInstance inst,
				  const VkAllocationCallbacks *ac)
{
	struct vk_inst_data *idata = (struct vk_inst_data *)remove_obj_data(
		&instances, (uint64_t)GET_LDT(inst));
	vk_free(ac, idata);
}

/* ======================================================================== */
/* capture                                                                  */

static inline bool vk_shtex_init_d3d11(struct vk_data *data)
{
	D3D_FEATURE_LEVEL level_used;
	IDXGIFactory1 *factory;
	IDXGIAdapter1 *adapter;
	HRESULT hr;

	HMODULE d3d11 = load_system_library("d3d11.dll");
	if (!d3d11) {
		flog("failed to load d3d11: %d", GetLastError());
		return false;
	}

	HMODULE dxgi = load_system_library("dxgi.dll");
	if (!dxgi) {
		flog("failed to load dxgi: %d", GetLastError());
		return false;
	}

	HRESULT(WINAPI * create_factory)
	(REFIID, void **) = (void *)GetProcAddress(dxgi, "CreateDXGIFactory1");
	if (!create_factory) {
		flog("failed to get CreateDXGIFactory1 address: %d",
		     GetLastError());
		return false;
	}

	PFN_D3D11_CREATE_DEVICE create =
		(void *)GetProcAddress(d3d11, "D3D11CreateDevice");
	if (!create) {
		flog("failed to get D3D11CreateDevice address: %d",
		     GetLastError());
		return false;
	}

	hr = create_factory(&IID_IDXGIFactory1, &factory);
	if (FAILED(hr)) {
		flog_hr("failed to create factory", hr);
		return false;
	}

	hr = IDXGIFactory1_EnumAdapters1(factory, 0, &adapter);
	IDXGIFactory1_Release(factory);

	if (FAILED(hr)) {
		flog_hr("failed to create adapter", hr);
		return false;
	}

	static const D3D_FEATURE_LEVEL feature_levels[] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
	};

	hr = create((IDXGIAdapter *)adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0,
		    feature_levels,
		    sizeof(feature_levels) / sizeof(D3D_FEATURE_LEVEL),
		    D3D11_SDK_VERSION, &data->d3d11_device, &level_used,
		    &data->d3d11_context);
	IDXGIAdapter1_Release(adapter);

	if (FAILED(hr)) {
		flog_hr("failed to create device", hr);
		return false;
	}

	return true;
}

static inline bool vk_shtex_init_d3d11_tex(struct vk_data *data,
					   struct vk_swap_data *swap)
{
	IDXGIResource *dxgi_res;
	HRESULT hr;

	const UINT width = swap->image_extent.width;
	const UINT height = swap->image_extent.height;

	flog("OBS requesting %s texture format. capture dimensions: %ux%u",
	     vk_format_to_str(swap->format), width, height);

	const DXGI_FORMAT format = vk_format_to_dxgi(swap->format);
	if (format == DXGI_FORMAT_UNKNOWN) {
		flog("cannot convert to DXGI format");
		return false;
	}

	D3D11_TEXTURE2D_DESC desc = {0};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = apply_dxgi_format_typeless(
		format, global_hook_info->allow_srgb_alias);
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	hr = ID3D11Device_CreateTexture2D(data->d3d11_device, &desc, NULL,
					  &swap->d3d11_tex);
	if (FAILED(hr)) {
		flog_hr("failed to create texture", hr);
		return false;
	}

	hr = ID3D11Texture2D_QueryInterface(swap->d3d11_tex, &IID_IDXGIResource,
					    &dxgi_res);
	if (FAILED(hr)) {
		flog_hr("failed to get IDXGIResource", hr);
		return false;
	}

	hr = IDXGIResource_GetSharedHandle(dxgi_res, &swap->handle);
	IDXGIResource_Release(dxgi_res);

	if (FAILED(hr)) {
		flog_hr("failed to get shared handle", hr);
		return false;
	}

	return true;
}

static inline bool vk_shtex_init_vulkan_tex(struct vk_data *data,
					    struct vk_swap_data *swap)
{
	struct vk_device_funcs *funcs = &data->funcs;
	VkExternalMemoryFeatureFlags f =
		data->external_mem_props.externalMemoryFeatures;

	/* -------------------------------------------------------- */
	/* create texture                                           */

	VkExternalMemoryImageCreateInfo emici;
	emici.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
	emici.pNext = NULL;
	emici.handleTypes =
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;

	VkImageCreateInfo ici;
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.pNext = &emici;
	ici.flags = 0;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = swap->format;
	ici.extent.width = swap->image_extent.width;
	ici.extent.height = swap->image_extent.height;
	ici.extent.depth = 1;
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		    VK_IMAGE_USAGE_SAMPLED_BIT;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.queueFamilyIndexCount = 0;
	ici.pQueueFamilyIndices = 0;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkDevice device = data->device;

	VkResult res;
	res = funcs->CreateImage(device, &ici, data->ac, &swap->export_image);
	if (VK_SUCCESS != res) {
		flog("failed to CreateImage: %s", result_to_str(res));
		swap->export_image = VK_NULL_HANDLE;
		return false;
	}

	swap->layout_initialized = false;

	/* -------------------------------------------------------- */
	/* get image memory requirements                            */

	VkMemoryRequirements mr;
	bool use_gimr2 = f & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT;

	if (use_gimr2) {
		VkMemoryDedicatedRequirements mdr = {0};
		mdr.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
		mdr.pNext = NULL;

		VkMemoryRequirements2 mr2 = {0};
		mr2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
		mr2.pNext = &mdr;

		VkImageMemoryRequirementsInfo2 imri2 = {0};
		imri2.sType =
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
		imri2.pNext = NULL;
		imri2.image = swap->export_image;

		funcs->GetImageMemoryRequirements2(device, &imri2, &mr2);
		mr = mr2.memoryRequirements;
	} else {
		funcs->GetImageMemoryRequirements(device, swap->export_image,
						  &mr);
	}

	/* -------------------------------------------------------- */
	/* get memory type index                                    */

	struct vk_inst_funcs *ifuncs =
		get_inst_funcs_by_physical_device(data->phy_device);

	VkPhysicalDeviceMemoryProperties pdmp;
	ifuncs->GetPhysicalDeviceMemoryProperties(data->phy_device, &pdmp);

	uint32_t mem_type_idx = 0;

	for (; mem_type_idx < pdmp.memoryTypeCount; mem_type_idx++) {
		if ((mr.memoryTypeBits & (1 << mem_type_idx)) &&
		    (pdmp.memoryTypes[mem_type_idx].propertyFlags &
		     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
			    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
			break;
		}
	}

	if (mem_type_idx == pdmp.memoryTypeCount) {
		flog("failed to get memory type index");
		funcs->DestroyImage(device, swap->export_image, data->ac);
		swap->export_image = VK_NULL_HANDLE;
		return false;
	}

	/* -------------------------------------------------------- */
	/* allocate memory                                          */

	VkImportMemoryWin32HandleInfoKHR imw32hi;
	imw32hi.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
	imw32hi.pNext = NULL;
	imw32hi.name = NULL;
	imw32hi.handleType =
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;
	imw32hi.handle = swap->handle;

	VkMemoryAllocateInfo mai;
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.pNext = &imw32hi;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_type_idx;

	VkMemoryDedicatedAllocateInfo mdai;
	mdai.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	mdai.pNext = NULL;
	mdai.buffer = VK_NULL_HANDLE;

	if (data->external_mem_props.externalMemoryFeatures &
	    VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) {
		mdai.image = swap->export_image;
		imw32hi.pNext = &mdai;
	}

	res = funcs->AllocateMemory(device, &mai, NULL, &swap->export_mem);
	if (VK_SUCCESS != res) {
		flog("failed to AllocateMemory: %s", result_to_str(res));
		funcs->DestroyImage(device, swap->export_image, data->ac);
		swap->export_image = VK_NULL_HANDLE;
		return false;
	}

	/* -------------------------------------------------------- */
	/* bind image memory                                        */

	bool use_bi2 = f & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT;

	if (use_bi2) {
		VkBindImageMemoryInfo bimi = {0};
		bimi.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
		bimi.image = swap->export_image;
		bimi.memory = swap->export_mem;
		bimi.memoryOffset = 0;
		res = funcs->BindImageMemory2(device, 1, &bimi);
	} else {
		res = funcs->BindImageMemory(device, swap->export_image,
					     swap->export_mem, 0);
	}
	if (VK_SUCCESS != res) {
		flog("%s failed: %s",
		     use_bi2 ? "BindImageMemory2" : "BindImageMemory",
		     result_to_str(res));
		funcs->DestroyImage(device, swap->export_image, data->ac);
		swap->export_image = VK_NULL_HANDLE;
		return false;
	}
	return true;
}

static bool vk_shtex_init(struct vk_data *data, HWND window,
			  struct vk_swap_data *swap)
{
	if (!vk_shtex_init_d3d11(data)) {
		return false;
	}
	if (!vk_shtex_init_d3d11_tex(data, swap)) {
		return false;
	}
	if (!vk_shtex_init_vulkan_tex(data, swap)) {
		return false;
	}

	data->cur_swap = swap;

	swap->captured = capture_init_shtex(&swap->shtex_info, window,
					    swap->image_extent.width,
					    swap->image_extent.height,
					    (uint32_t)swap->format, false,
					    (uintptr_t)swap->handle);

	if (!swap->captured)
		return false;

	if (global_hook_info->force_shmem) {
		flog("shared memory capture currently "
		     "unsupported; ignoring");
	}

	hlog("vulkan shared texture capture successful");
	return true;
}

static void vk_shtex_create_frame_objects(struct vk_data *data,
					  struct vk_queue_data *queue_data,
					  uint32_t image_count)
{
	queue_data->frames =
		vk_alloc(data->ac, image_count * sizeof(struct vk_frame_data),
			 _Alignof(struct vk_frame_data),
			 VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
	memset(queue_data->frames, 0,
	       image_count * sizeof(struct vk_frame_data));
	queue_data->frame_index = 0;
	queue_data->frame_count = image_count;

	VkDevice device = data->device;
	for (uint32_t image_index = 0; image_index < image_count;
	     image_index++) {
		struct vk_frame_data *frame_data =
			&queue_data->frames[image_index];

		VkCommandPoolCreateInfo cpci;
		cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cpci.pNext = NULL;
		cpci.flags = 0;
		cpci.queueFamilyIndex = queue_data->fam_idx;

		VkResult res = data->funcs.CreateCommandPool(
			device, &cpci, data->ac, &frame_data->cmd_pool);
		debug_res("CreateCommandPool", res);

		VkCommandBufferAllocateInfo cbai;
		cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cbai.pNext = NULL;
		cbai.commandPool = frame_data->cmd_pool;
		cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cbai.commandBufferCount = 1;

		res = data->funcs.AllocateCommandBuffers(
			device, &cbai, &frame_data->cmd_buffer);
		debug_res("AllocateCommandBuffers", res);
		GET_LDT(frame_data->cmd_buffer) = GET_LDT(device);

		VkFenceCreateInfo fci = {0};
		fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fci.pNext = NULL;
		fci.flags = 0;
		res = data->funcs.CreateFence(device, &fci, data->ac,
					      &frame_data->fence);
		debug_res("CreateFence", res);
	}
}

static void vk_shtex_destroy_fence(struct vk_data *data, bool *cmd_buffer_busy,
				   VkFence *fence)
{
	VkDevice device = data->device;

	if (*cmd_buffer_busy) {
		data->funcs.WaitForFences(device, 1, fence, VK_TRUE, ~0ull);
		*cmd_buffer_busy = false;
	}

	data->funcs.DestroyFence(device, *fence, data->ac);
	*fence = VK_NULL_HANDLE;
}

static void vk_shtex_destroy_frame_objects(struct vk_data *data,
					   struct vk_queue_data *queue_data)
{
	VkDevice device = data->device;

	for (uint32_t frame_idx = 0; frame_idx < queue_data->frame_count;
	     frame_idx++) {
		struct vk_frame_data *frame_data =
			&queue_data->frames[frame_idx];
		bool *cmd_buffer_busy = &frame_data->cmd_buffer_busy;
		VkFence *fence = &frame_data->fence;
		vk_shtex_destroy_fence(data, cmd_buffer_busy, fence);

		data->funcs.DestroyCommandPool(device, frame_data->cmd_pool,
					       data->ac);
		frame_data->cmd_pool = VK_NULL_HANDLE;
	}

	vk_free(data->ac, queue_data->frames);
	queue_data->frames = NULL;
	queue_data->frame_count = 0;
}

static void vk_shtex_capture(struct vk_data *data,
			     struct vk_device_funcs *funcs,
			     struct vk_swap_data *swap, uint32_t idx,
			     VkQueue queue, const VkPresentInfoKHR *info)
{
	VkResult res = VK_SUCCESS;

	VkCommandBufferBeginInfo begin_info;
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = NULL;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = NULL;

	VkImageMemoryBarrier mb[2];
	VkImageMemoryBarrier *src_mb = &mb[0];
	VkImageMemoryBarrier *dst_mb = &mb[1];

	/* ------------------------------------------------------ */
	/* do image copy                                          */

	const uint32_t image_index = info->pImageIndices[idx];
	VkImage cur_backbuffer = swap->swap_images[image_index];

	struct vk_queue_data *queue_data = get_queue_data(data, queue);
	uint32_t fam_idx = queue_data->fam_idx;

	const uint32_t image_count = swap->image_count;
	if (queue_data->frame_count < image_count) {
		if (queue_data->frame_count > 0)
			vk_shtex_destroy_frame_objects(data, queue_data);
		vk_shtex_create_frame_objects(data, queue_data, image_count);
	}

	const uint32_t frame_index = queue_data->frame_index;
	struct vk_frame_data *frame_data = &queue_data->frames[frame_index];
	queue_data->frame_index = (frame_index + 1) % queue_data->frame_count;
	vk_shtex_clear_fence(data, frame_data);

	VkDevice device = data->device;

	res = funcs->ResetCommandPool(device, frame_data->cmd_pool, 0);

#ifdef MORE_DEBUGGING
	debug_res("ResetCommandPool", res);
#endif

	const VkCommandBuffer cmd_buffer = frame_data->cmd_buffer;
	res = funcs->BeginCommandBuffer(cmd_buffer, &begin_info);

#ifdef MORE_DEBUGGING
	debug_res("BeginCommandBuffer", res);
#endif

	/* ------------------------------------------------------ */
	/* transition shared texture if necessary                 */

	if (!swap->layout_initialized) {
		VkImageMemoryBarrier imb;
		imb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imb.pNext = NULL;
		imb.srcAccessMask = 0;
		imb.dstAccessMask = 0;
		imb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imb.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imb.image = swap->export_image;
		imb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imb.subresourceRange.baseMipLevel = 0;
		imb.subresourceRange.levelCount = 1;
		imb.subresourceRange.baseArrayLayer = 0;
		imb.subresourceRange.layerCount = 1;

		funcs->CmdPipelineBarrier(cmd_buffer,
					  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
					  0, NULL, 0, NULL, 1, &imb);

		swap->layout_initialized = true;
	}

	/* ------------------------------------------------------ */
	/* transition cur_backbuffer to transfer source state     */

	src_mb->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	src_mb->pNext = NULL;
	src_mb->srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	src_mb->dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	src_mb->oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	src_mb->newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	src_mb->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	src_mb->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	src_mb->image = cur_backbuffer;
	src_mb->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	src_mb->subresourceRange.baseMipLevel = 0;
	src_mb->subresourceRange.levelCount = 1;
	src_mb->subresourceRange.baseArrayLayer = 0;
	src_mb->subresourceRange.layerCount = 1;

	/* ------------------------------------------------------ */
	/* transition exportedTexture to transfer dest state      */

	dst_mb->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	dst_mb->pNext = NULL;
	dst_mb->srcAccessMask = 0;
	dst_mb->dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	dst_mb->oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	dst_mb->newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	dst_mb->srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
	dst_mb->dstQueueFamilyIndex = fam_idx;
	dst_mb->image = swap->export_image;
	dst_mb->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	dst_mb->subresourceRange.baseMipLevel = 0;
	dst_mb->subresourceRange.levelCount = 1;
	dst_mb->subresourceRange.baseArrayLayer = 0;
	dst_mb->subresourceRange.layerCount = 1;

	funcs->CmdPipelineBarrier(cmd_buffer,
				  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				  VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0,
				  NULL, 2, mb);

	/* ------------------------------------------------------ */
	/* copy cur_backbuffer's content to our interop image     */

	VkImageCopy cpy;
	cpy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cpy.srcSubresource.mipLevel = 0;
	cpy.srcSubresource.baseArrayLayer = 0;
	cpy.srcSubresource.layerCount = 1;
	cpy.srcOffset.x = 0;
	cpy.srcOffset.y = 0;
	cpy.srcOffset.z = 0;
	cpy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cpy.dstSubresource.mipLevel = 0;
	cpy.dstSubresource.baseArrayLayer = 0;
	cpy.dstSubresource.layerCount = 1;
	cpy.dstOffset.x = 0;
	cpy.dstOffset.y = 0;
	cpy.dstOffset.z = 0;
	cpy.extent.width = swap->image_extent.width;
	cpy.extent.height = swap->image_extent.height;
	cpy.extent.depth = 1;
	funcs->CmdCopyImage(cmd_buffer, cur_backbuffer,
			    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			    swap->export_image,
			    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);

	/* ------------------------------------------------------ */
	/* Restore the swap chain image layout to what it was 
	 * before.  This may not be strictly needed, but it is
	 * generally good to restore things to their original
	 * state.  */

	src_mb->srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	src_mb->dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	src_mb->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	src_mb->newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	dst_mb->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	dst_mb->dstAccessMask = 0;
	dst_mb->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	dst_mb->newLayout = VK_IMAGE_LAYOUT_GENERAL;
	dst_mb->srcQueueFamilyIndex = fam_idx;
	dst_mb->dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;

	funcs->CmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
				  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT |
					  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				  0, 0, NULL, 0, NULL, 2, mb);

	funcs->EndCommandBuffer(cmd_buffer);

	/* ------------------------------------------------------ */

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 0;
	submit_info.pWaitSemaphores = NULL;
	submit_info.pWaitDstStageMask = NULL;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buffer;
	submit_info.signalSemaphoreCount = 0;
	submit_info.pSignalSemaphores = NULL;

	const VkFence fence = frame_data->fence;
	res = funcs->QueueSubmit(queue, 1, &submit_info, fence);

#ifdef MORE_DEBUGGING
	debug_res("QueueSubmit", res);
#endif

	if (res == VK_SUCCESS)
		frame_data->cmd_buffer_busy = true;
}

static inline bool valid_rect(struct vk_swap_data *swap)
{
	return !!swap->image_extent.width && !!swap->image_extent.height;
}

static void vk_capture(struct vk_data *data, VkQueue queue,
		       const VkPresentInfoKHR *info)
{
	struct vk_swap_data *swap = NULL;
	HWND window = NULL;
	uint32_t idx = 0;

#ifdef MORE_DEBUGGING
	debug("QueuePresentKHR called on "
	      "devicekey %p, swapchain count %d",
	      &data->funcs, info->swapchainCount);
#endif

	/* use first swap chain associated with a window */
	for (; idx < info->swapchainCount; idx++) {
		struct vk_swap_data *cur_swap =
			get_swap_data(data, info->pSwapchains[idx]);
		if (cur_swap) {
			window = cur_swap->hwnd;
			if (window != NULL) {
				swap = cur_swap;
				break;
			}
		}
	}

	if (!window) {
		return;
	}

	if (capture_should_stop()) {
		vk_shtex_free(data);
	}
	if (capture_should_init()) {
		if (valid_rect(swap) && !vk_shtex_init(data, window, swap)) {
			vk_shtex_free(data);
			data->valid = false;
			flog("vk_shtex_init failed");
		}
	}
	if (capture_ready()) {
		if (swap != data->cur_swap) {
			vk_shtex_free(data);
			return;
		}

		vk_shtex_capture(data, &data->funcs, swap, idx, queue, info);
	}
}

static VkResult VKAPI_CALL OBS_QueuePresentKHR(VkQueue queue,
					       const VkPresentInfoKHR *info)
{
	struct vk_data *const data = get_device_data_by_queue(queue);
	struct vk_queue_data *const queue_data = get_queue_data(data, queue);
	struct vk_device_funcs *const funcs = &data->funcs;

	if (data->valid && queue_data->supports_transfer) {
		vk_capture(data, queue, info);
	}

	return funcs->QueuePresentKHR(queue, info);
}

/* ======================================================================== */
/* setup hooks                                                              */

static inline bool is_inst_link_info(VkLayerInstanceCreateInfo *lici)
{
	return lici->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO &&
	       lici->function == VK_LAYER_LINK_INFO;
}

static VkResult VKAPI_CALL OBS_CreateInstance(const VkInstanceCreateInfo *cinfo,
					      const VkAllocationCallbacks *ac,
					      VkInstance *p_inst)
{
	VkInstanceCreateInfo info = *cinfo;

	/* -------------------------------------------------------- */
	/* step through chain until we get to the link info         */

	VkLayerInstanceCreateInfo *lici = (void *)info.pNext;
	while (lici && !is_inst_link_info(lici)) {
		lici = (VkLayerInstanceCreateInfo *)lici->pNext;
	}

	if (lici == NULL) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	PFN_vkGetInstanceProcAddr gpa =
		lici->u.pLayerInfo->pfnNextGetInstanceProcAddr;

	/* -------------------------------------------------------- */
	/* move chain on for next layer                             */

	lici->u.pLayerInfo = lici->u.pLayerInfo->pNext;

	/* -------------------------------------------------------- */
	/* (HACK) Set api version to 1.1 if set to 1.0              */
	/* We do this to get our extensions working properly        */

	VkApplicationInfo ai;
	if (info.pApplicationInfo) {
		ai = *info.pApplicationInfo;
		if (ai.apiVersion < VK_API_VERSION_1_1)
			ai.apiVersion = VK_API_VERSION_1_1;
	} else {
		ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		ai.pNext = NULL;
		ai.pApplicationName = NULL;
		ai.applicationVersion = 0;
		ai.pEngineName = NULL;
		ai.engineVersion = 0;
		ai.apiVersion = VK_API_VERSION_1_1;
	}

	info.pApplicationInfo = &ai;

	/* -------------------------------------------------------- */
	/* allocate data node                                       */

	struct vk_inst_data *idata = alloc_inst_data(ac);
	if (!idata)
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	/* -------------------------------------------------------- */
	/* create instance                                          */

	PFN_vkCreateInstance create = (void *)gpa(NULL, "vkCreateInstance");

	VkResult res = create(&info, ac, p_inst);
	bool valid = res == VK_SUCCESS;
	if (!valid) {
		/* try again with original arguments */
		res = create(cinfo, ac, p_inst);
		if (res != VK_SUCCESS) {
			vk_free(ac, idata);
			return res;
		}
	}

	VkInstance inst = *p_inst;
	init_inst_data(idata, inst);

	/* -------------------------------------------------------- */
	/* fetch the functions we need                              */

	struct vk_inst_funcs *ifuncs = &idata->funcs;

#define GETADDR(x)                                      \
	do {                                            \
		ifuncs->x = (void *)gpa(inst, "vk" #x); \
		if (!ifuncs->x) {                       \
			flog("could not get instance "  \
			     "address for vk" #x);      \
			funcs_found = false;            \
		}                                       \
	} while (false)

	bool funcs_found = true;
	GETADDR(GetInstanceProcAddr);
	GETADDR(DestroyInstance);
	GETADDR(CreateWin32SurfaceKHR);
	GETADDR(DestroySurfaceKHR);
	GETADDR(GetPhysicalDeviceQueueFamilyProperties);
	GETADDR(GetPhysicalDeviceMemoryProperties);
	GETADDR(GetPhysicalDeviceImageFormatProperties2);
	GETADDR(EnumerateDeviceExtensionProperties);
#undef GETADDR

	valid = valid && funcs_found;
	idata->valid = valid;

	if (valid)
		init_obj_list(&idata->surfaces);

	return res;
}

static void VKAPI_CALL OBS_DestroyInstance(VkInstance instance,
					   const VkAllocationCallbacks *ac)
{
	struct vk_inst_funcs *ifuncs = get_inst_funcs(instance);
	PFN_vkDestroyInstance destroy_instance = ifuncs->DestroyInstance;

	remove_free_inst_data(instance, ac);

	destroy_instance(instance, ac);
}

static bool
vk_shared_tex_supported(struct vk_inst_funcs *funcs,
			VkPhysicalDevice phy_device, VkFormat format,
			VkImageUsageFlags usage,
			VkExternalMemoryProperties *external_mem_props)
{
	VkPhysicalDeviceImageFormatInfo2 info;
	VkPhysicalDeviceExternalImageFormatInfo external_info;

	external_info.sType =
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
	external_info.pNext = NULL;
	external_info.handleType =
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;

	info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
	info.pNext = &external_info;
	info.format = format;
	info.type = VK_IMAGE_TYPE_2D;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.flags = 0;
	info.usage = usage;

	VkExternalImageFormatProperties external_props = {0};
	external_props.sType =
		VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
	external_props.pNext = NULL;

	VkImageFormatProperties2 props = {0};
	props.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
	props.pNext = &external_props;

	VkResult result = funcs->GetPhysicalDeviceImageFormatProperties2(
		phy_device, &info, &props);

	*external_mem_props = external_props.externalMemoryProperties;

	const VkExternalMemoryFeatureFlags features =
		external_mem_props->externalMemoryFeatures;

	return ((VK_SUCCESS == result) &&
		(features & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT));
}

static inline bool is_device_link_info(VkLayerDeviceCreateInfo *lici)
{
	return lici->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO &&
	       lici->function == VK_LAYER_LINK_INFO;
}

static VkResult VKAPI_CALL OBS_CreateDevice(VkPhysicalDevice phy_device,
					    const VkDeviceCreateInfo *info,
					    const VkAllocationCallbacks *ac,
					    VkDevice *p_device)
{
	struct vk_inst_data *idata =
		get_inst_data_by_physical_device(phy_device);
	struct vk_inst_funcs *ifuncs = &idata->funcs;
	struct vk_data *data = NULL;

	VkResult ret = VK_ERROR_INITIALIZATION_FAILED;

	VkLayerDeviceCreateInfo *ldci = (void *)info->pNext;

	/* -------------------------------------------------------- */
	/* step through chain until we get to the link info         */

	while (ldci && !is_device_link_info(ldci)) {
		ldci = (VkLayerDeviceCreateInfo *)ldci->pNext;
	}

	if (!ldci) {
		goto fail;
	}

	PFN_vkGetInstanceProcAddr gipa;
	PFN_vkGetDeviceProcAddr gdpa;

	gipa = ldci->u.pLayerInfo->pfnNextGetInstanceProcAddr;
	gdpa = ldci->u.pLayerInfo->pfnNextGetDeviceProcAddr;

	/* -------------------------------------------------------- */
	/* move chain on for next layer                             */

	ldci->u.pLayerInfo = ldci->u.pLayerInfo->pNext;

	/* -------------------------------------------------------- */
	/* allocate data node                                       */

	data = alloc_device_data(ac);
	if (!data)
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	init_obj_list(&data->queues);

	/* -------------------------------------------------------- */
	/* create device and initialize hook data                   */

	PFN_vkCreateDevice createFunc =
		(PFN_vkCreateDevice)gipa(idata->instance, "vkCreateDevice");

	ret = createFunc(phy_device, info, ac, p_device);
	if (ret != VK_SUCCESS) {
		vk_free(ac, data);
		return ret;
	}

	VkDevice device = *p_device;
	init_device_data(data, device);

	data->valid = false; /* set true below if it doesn't go to fail */
	data->phy_device = phy_device;

	/* -------------------------------------------------------- */
	/* fetch the functions we need                              */

	struct vk_device_funcs *dfuncs = &data->funcs;
	bool funcs_found = true;

#define GETADDR(x)                                         \
	do {                                               \
		dfuncs->x = (void *)gdpa(device, "vk" #x); \
		if (!dfuncs->x) {                          \
			flog("could not get device "       \
			     "address for vk" #x);         \
			funcs_found = false;               \
		}                                          \
	} while (false)

	GETADDR(GetDeviceProcAddr);
	GETADDR(DestroyDevice);
	GETADDR(CreateSwapchainKHR);
	GETADDR(DestroySwapchainKHR);
	GETADDR(QueuePresentKHR);
	GETADDR(AllocateMemory);
	GETADDR(FreeMemory);
	GETADDR(BindImageMemory);
	GETADDR(BindImageMemory2);
	GETADDR(GetSwapchainImagesKHR);
	GETADDR(CreateImage);
	GETADDR(DestroyImage);
	GETADDR(GetImageMemoryRequirements);
	GETADDR(GetImageMemoryRequirements2);
	GETADDR(ResetCommandPool);
	GETADDR(BeginCommandBuffer);
	GETADDR(EndCommandBuffer);
	GETADDR(CmdCopyImage);
	GETADDR(CmdPipelineBarrier);
	GETADDR(GetDeviceQueue);
	GETADDR(QueueSubmit);
	GETADDR(CreateCommandPool);
	GETADDR(DestroyCommandPool);
	GETADDR(AllocateCommandBuffers);
	GETADDR(CreateFence);
	GETADDR(DestroyFence);
	GETADDR(WaitForFences);
	GETADDR(ResetFences);
#undef GETADDR

	if (!funcs_found) {
		goto fail;
	}

	if (!idata->valid) {
		flog("instance not valid");
		goto fail;
	}

	const char *required_device_extensions[] = {
		VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME};

	uint32_t device_extension_count = 0;
	ret = ifuncs->EnumerateDeviceExtensionProperties(
		phy_device, NULL, &device_extension_count, NULL);
	if (ret != VK_SUCCESS)
		goto fail;

	VkExtensionProperties *device_extensions = _malloca(
		sizeof(VkExtensionProperties) * device_extension_count);
	ret = ifuncs->EnumerateDeviceExtensionProperties(
		phy_device, NULL, &device_extension_count, device_extensions);
	if (ret != VK_SUCCESS) {
		_freea(device_extensions);
		goto fail;
	}

	bool extensions_found = true;
	for (uint32_t i = 0; i < _countof(required_device_extensions); i++) {
		const char *const required_extension =
			required_device_extensions[i];

		bool found = false;
		for (uint32_t j = 0; j < device_extension_count; j++) {
			if (!strcmp(required_extension,
				    device_extensions[j].extensionName)) {
				found = true;
				break;
			}
		}

		if (!found) {
			flog("missing device extension: %s",
			     required_extension);
			extensions_found = false;
		}
	}

	_freea(device_extensions);

	if (!extensions_found)
		goto fail;

	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				  VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if (!vk_shared_tex_supported(ifuncs, phy_device, format, usage,
				     &data->external_mem_props)) {
		flog("texture sharing is not supported");
		goto fail;
	}

	data->inst_data = idata;

	data->ac = NULL;
	if (ac) {
		data->ac_storage = *ac;
		data->ac = &data->ac_storage;
	}

	uint32_t queue_family_property_count = 0;
	ifuncs->GetPhysicalDeviceQueueFamilyProperties(
		phy_device, &queue_family_property_count, NULL);
	VkQueueFamilyProperties *queue_family_properties = _malloca(
		sizeof(VkQueueFamilyProperties) * queue_family_property_count);
	ifuncs->GetPhysicalDeviceQueueFamilyProperties(
		phy_device, &queue_family_property_count,
		queue_family_properties);

	for (uint32_t info_index = 0, info_count = info->queueCreateInfoCount;
	     info_index < info_count; ++info_index) {
		const VkDeviceQueueCreateInfo *queue_info =
			&info->pQueueCreateInfos[info_index];
		for (uint32_t queue_index = 0,
			      queue_count = queue_info->queueCount;
		     queue_index < queue_count; ++queue_index) {
			const uint32_t family_index =
				queue_info->queueFamilyIndex;
			VkQueue queue;
			data->funcs.GetDeviceQueue(device, family_index,
						   queue_index, &queue);
			const bool supports_transfer =
				(queue_family_properties[family_index]
					 .queueFlags &
				 (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT |
				  VK_QUEUE_TRANSFER_BIT)) != 0;
			add_queue_data(data, queue, family_index,
				       supports_transfer, ac);
		}
	}

	_freea(queue_family_properties);

	init_obj_list(&data->swaps);
	data->cur_swap = NULL;
	data->d3d11_device = NULL;
	data->d3d11_context = NULL;

	data->valid = true;

fail:
	return ret;
}

static void VKAPI_CALL OBS_DestroyDevice(VkDevice device,
					 const VkAllocationCallbacks *ac)
{
	struct vk_data *data = remove_device_data(device);

	if (data->valid) {
		struct vk_queue_data *queue_data = queue_walk_begin(data);

		while (queue_data) {
			vk_shtex_destroy_frame_objects(data, queue_data);

			queue_data = queue_walk_next(queue_data);
		}

		queue_walk_end(data);

		remove_free_queue_all(data, ac);
	}

	PFN_vkDestroyDevice destroy_device = data->funcs.DestroyDevice;

	vk_free(ac, data);

	destroy_device(device, ac);
}

static VkResult VKAPI_CALL
OBS_CreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *cinfo,
		       const VkAllocationCallbacks *ac, VkSwapchainKHR *p_sc)
{
	struct vk_data *data = get_device_data(device);
	struct vk_device_funcs *funcs = &data->funcs;
	if (!data->valid)
		return funcs->CreateSwapchainKHR(device, cinfo, ac, p_sc);

	VkSwapchainCreateInfoKHR info = *cinfo;
	info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	VkResult res = funcs->CreateSwapchainKHR(device, &info, ac, p_sc);
	debug_res("CreateSwapchainKHR", res);
	if (res != VK_SUCCESS) {
		/* try again with original imageUsage flags */
		return funcs->CreateSwapchainKHR(device, cinfo, ac, p_sc);
	}

	VkSwapchainKHR sc = *p_sc;
	uint32_t count = 0;
	res = funcs->GetSwapchainImagesKHR(device, sc, &count, NULL);
	debug_res("GetSwapchainImagesKHR", res);
	if ((res == VK_SUCCESS) && (count > 0)) {
		struct vk_swap_data *swap_data = alloc_swap_data(ac);
		if (swap_data) {
			init_swap_data(swap_data, data, sc);
			swap_data->swap_images = vk_alloc(
				ac, count * sizeof(VkImage), _Alignof(VkImage),
				VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
			res = funcs->GetSwapchainImagesKHR(
				device, sc, &count, swap_data->swap_images);
			debug_res("GetSwapchainImagesKHR", res);

			swap_data->image_extent = cinfo->imageExtent;
			swap_data->format = cinfo->imageFormat;
			swap_data->hwnd =
				find_surf_hwnd(data->inst_data, cinfo->surface);
			swap_data->export_image = VK_NULL_HANDLE;
			swap_data->layout_initialized = false;
			swap_data->export_mem = VK_NULL_HANDLE;
			swap_data->image_count = count;
			swap_data->handle = INVALID_HANDLE_VALUE;
			swap_data->shtex_info = NULL;
			swap_data->d3d11_tex = NULL;
			swap_data->captured = false;
		}
	}

	return VK_SUCCESS;
}

static void VKAPI_CALL OBS_DestroySwapchainKHR(VkDevice device,
					       VkSwapchainKHR sc,
					       const VkAllocationCallbacks *ac)
{
	struct vk_data *data = get_device_data(device);
	struct vk_device_funcs *funcs = &data->funcs;
	PFN_vkDestroySwapchainKHR destroy_swapchain =
		funcs->DestroySwapchainKHR;

	if ((sc != VK_NULL_HANDLE) && data->valid) {
		struct vk_swap_data *swap = get_swap_data(data, sc);
		if (swap) {
			if (data->cur_swap == swap) {
				vk_shtex_free(data);
			}

			vk_free(ac, swap->swap_images);

			remove_free_swap_data(data, sc, ac);
		}
	}

	destroy_swapchain(device, sc, ac);
}

static VkResult VKAPI_CALL OBS_CreateWin32SurfaceKHR(
	VkInstance inst, const VkWin32SurfaceCreateInfoKHR *info,
	const VkAllocationCallbacks *ac, VkSurfaceKHR *surf)
{
	struct vk_inst_data *idata = get_inst_data(inst);
	struct vk_inst_funcs *ifuncs = &idata->funcs;

	VkResult res = ifuncs->CreateWin32SurfaceKHR(inst, info, ac, surf);
	if ((res == VK_SUCCESS) && idata->valid)
		add_surf_data(idata, *surf, info->hwnd, ac);
	return res;
}

static void VKAPI_CALL OBS_DestroySurfaceKHR(VkInstance inst, VkSurfaceKHR surf,
					     const VkAllocationCallbacks *ac)
{
	struct vk_inst_data *idata = get_inst_data(inst);
	struct vk_inst_funcs *ifuncs = &idata->funcs;
	PFN_vkDestroySurfaceKHR destroy_surface = ifuncs->DestroySurfaceKHR;

	if ((surf != VK_NULL_HANDLE) && idata->valid)
		remove_free_surf_data(idata, surf, ac);

	destroy_surface(inst, surf, ac);
}

#define GETPROCADDR(func)               \
	if (!strcmp(pName, "vk" #func)) \
		return (PFN_vkVoidFunction)&OBS_##func;

#define GETPROCADDR_IF_SUPPORTED(func)  \
	if (!strcmp(pName, "vk" #func)) \
		return funcs->func ? (PFN_vkVoidFunction)&OBS_##func : NULL;

static PFN_vkVoidFunction VKAPI_CALL OBS_GetDeviceProcAddr(VkDevice device,
							   const char *pName)
{
	struct vk_data *data = get_device_data(device);
	struct vk_device_funcs *funcs = &data->funcs;

	debug_procaddr("vkGetDeviceProcAddr(%p, \"%s\")", device, pName);

	GETPROCADDR(GetDeviceProcAddr);
	GETPROCADDR(DestroyDevice);
	GETPROCADDR_IF_SUPPORTED(CreateSwapchainKHR);
	GETPROCADDR_IF_SUPPORTED(DestroySwapchainKHR);
	GETPROCADDR_IF_SUPPORTED(QueuePresentKHR);

	if (funcs->GetDeviceProcAddr == NULL)
		return NULL;
	return funcs->GetDeviceProcAddr(device, pName);
}

/* bad layers require spec violation */
#define RETURN_FP_FOR_NULL_INSTANCE 1

static PFN_vkVoidFunction VKAPI_CALL
OBS_GetInstanceProcAddr(VkInstance instance, const char *pName)
{
	debug_procaddr("vkGetInstanceProcAddr(%p, \"%s\")", instance, pName);

	/* instance chain functions we intercept */
	GETPROCADDR(GetInstanceProcAddr);
	GETPROCADDR(CreateInstance);

#if RETURN_FP_FOR_NULL_INSTANCE
	/* other instance chain functions we intercept */
	GETPROCADDR(DestroyInstance);
	GETPROCADDR(CreateWin32SurfaceKHR);
	GETPROCADDR(DestroySurfaceKHR);

	/* device chain functions we intercept */
	GETPROCADDR(GetDeviceProcAddr);
	GETPROCADDR(CreateDevice);
	GETPROCADDR(DestroyDevice);

	if (instance == NULL)
		return NULL;

	struct vk_inst_funcs *const funcs = get_inst_funcs(instance);
#else
	if (instance == NULL)
		return NULL;

	struct vk_inst_funcs *const funcs = get_inst_funcs(instance);

	/* other instance chain functions we intercept */
	GETPROCADDR(DestroyInstance);
	GETPROCADDR_IF_SUPPORTED(CreateWin32SurfaceKHR);
	GETPROCADDR_IF_SUPPORTED(DestroySurfaceKHR);

	/* device chain functions we intercept */
	GETPROCADDR(GetDeviceProcAddr);
	GETPROCADDR(CreateDevice);
	GETPROCADDR(DestroyDevice);
#endif

	const PFN_vkGetInstanceProcAddr gipa = funcs->GetInstanceProcAddr;
	return gipa ? gipa(instance, pName) : NULL;
}

#undef GETPROCADDR

#ifndef _WIN64
#pragma comment(linker, "/EXPORT:OBS_Negotiate=_OBS_Negotiate@4")
#endif

__declspec(dllexport) VkResult VKAPI_CALL
	OBS_Negotiate(VkNegotiateLayerInterface *nli)
{
	if (nli->loaderLayerInterfaceVersion >= 2) {
		nli->sType = LAYER_NEGOTIATE_INTERFACE_STRUCT;
		nli->pNext = NULL;
		nli->pfnGetInstanceProcAddr = OBS_GetInstanceProcAddr;
		nli->pfnGetDeviceProcAddr = OBS_GetDeviceProcAddr;
		nli->pfnGetPhysicalDeviceProcAddr = NULL;
	}

	const uint32_t cur_ver = CURRENT_LOADER_LAYER_INTERFACE_VERSION;

	if (nli->loaderLayerInterfaceVersion > cur_ver) {
		nli->loaderLayerInterfaceVersion = cur_ver;
	}

	if (!vulkan_seen) {
		init_obj_list(&instances);
		init_obj_list(&devices);

		vulkan_seen = true;
	}

	return VK_SUCCESS;
}

bool hook_vulkan(void)
{
	static bool hooked = false;
	if (!hooked && vulkan_seen) {
		hlog("Hooked Vulkan");
		hooked = true;
	}
	return hooked;
}
