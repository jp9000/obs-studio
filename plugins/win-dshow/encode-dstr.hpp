#pragma once

#include <util/util.hpp>

static inline void encode_dstr(struct dstr *str)
{
	dstr_replace(str, "#", "#22");
	dstr_replace(str, ":", "#3A");
}

static inline void decode_dstr(struct dstr *str)
{
	dstr_replace(str, "#3A", ":");
	dstr_replace(str, "#22", "#");
}

static inline void EncodeDeviceId(struct dstr *encodedStr,
				  const wchar_t *name_str,
				  const wchar_t *path_str)
{
	DStr name;
	DStr path;

	dstr_from_wcs(name, name_str);
	dstr_from_wcs(path, path_str);

	encode_dstr(name);
	encode_dstr(path);

	dstr_copy_dstr(encodedStr, name);
	dstr_cat(encodedStr, ":");
	dstr_cat_dstr(encodedStr, path);
}

static inline bool DecodeDeviceDStr(DStr &name, DStr &path,
				    const char *device_id)
{
	const char *path_str;

	if (!device_id || !*device_id)
		return false;

	path_str = strchr(device_id, ':');
	if (!path_str)
		return false;

	dstr_copy(path, path_str + 1);
	dstr_copy(name, device_id);

	size_t len = path_str - device_id;
	name->array[len] = 0;
	name->len = len;

	decode_dstr(name);
	decode_dstr(path);

	return true;
}

static inline bool DecodeDeviceId(DShow::DeviceId &out, const char *device_id)
{
	DStr name, path;

	if (!DecodeDeviceDStr(name, path, device_id))
		return false;

	BPtr<wchar_t> wname = dstr_to_wcs(name);
	out.name = wname;

	if (!dstr_is_empty(path)) {
		BPtr<wchar_t> wpath = dstr_to_wcs(path);
		out.path = wpath;
	}

	return true;
}
