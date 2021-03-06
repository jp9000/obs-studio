#pragma once

#include <stdint.h>

struct cached_cursor {
	gs_texture_t *texture;
	uint32_t cx;
	uint32_t cy;
};

struct cursor_data {
	gs_texture_t *texture;
	HCURSOR current_cursor;
	POINT cursor_pos;
	long x_hotspot;
	long y_hotspot;
	bool visible;

	uint32_t last_cx;
	uint32_t last_cy;

	DARRAY(struct cached_cursor) cached_textures;
};

extern void cursor_capture(struct cursor_data *data);
extern void cursor_draw(struct cursor_data *data, long x_offset, long y_offset,
			long width, long height);
extern void cursor_data_free(struct cursor_data *data);
