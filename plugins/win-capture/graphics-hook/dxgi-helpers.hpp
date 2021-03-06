#pragma once

static inline DXGI_FORMAT strip_dxgi_format_srgb(DXGI_FORMAT format)
{
	switch ((unsigned long)format) {
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	return format;
}

static inline DXGI_FORMAT apply_dxgi_format_typeless(DXGI_FORMAT format,
						     bool allow_srgb_alias)
{
	if (allow_srgb_alias) {
		switch ((unsigned long)format) {
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			return DXGI_FORMAT_B8G8R8A8_TYPELESS;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			return DXGI_FORMAT_R8G8B8A8_TYPELESS;
		}
	}

	return format;
}
