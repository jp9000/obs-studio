#pragma once

#include <util/darray.h>
#include <util/dstr.h>

struct device_item {
	struct dstr name, value;
};

static inline void device_item_free(struct device_item *item)
{
	dstr_free(&item->name);
	dstr_free(&item->value);
}

struct device_list {
	DARRAY(struct device_item) items;
};

static inline void device_list_free(struct device_list *list)
{
	for (size_t i = 0; i < list->items.num; i++)
		device_item_free(list->items.array + i);

	da_free(list->items);
}

static inline void device_list_add(struct device_list *list,
				   struct device_item *item)
{
	da_push_back(list->items, item);
	memset(item, 0, sizeof(struct device_item));
}

extern void coreaudio_enum_devices(struct device_list *list, bool input);
extern bool coreaudio_get_device_id(CFStringRef uid, AudioDeviceID *id);
