#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <obs-module.h>

struct dc_capture {
	gs_texture_t *texture;
	bool texture_written;
	int x, y;
	uint32_t width;
	uint32_t height;

	bool compatibility;
	HDC hdc;
	HBITMAP bmp, old_bmp;
	BYTE *bits;

	bool capture_cursor;
	bool cursor_captured;
	bool cursor_hidden;
	CURSORINFO ci;

	bool valid;
};

extern void dc_capture_init(struct dc_capture *capture, int x, int y,
			    uint32_t width, uint32_t height, bool cursor,
			    bool compatibility);
extern void dc_capture_free(struct dc_capture *capture);

extern void dc_capture_capture(struct dc_capture *capture, HWND window);
extern void dc_capture_render(struct dc_capture *capture, gs_effect_t *effect);
