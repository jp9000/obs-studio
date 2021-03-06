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

#include "gl-subsystem.h"

static inline bool init_ib(struct gs_index_buffer *ib)
{
	GLenum usage = ib->dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
	bool success;

	success = gl_create_buffer(GL_ELEMENT_ARRAY_BUFFER, &ib->buffer,
				   ib->size, ib->data, usage);

	if (!ib->dynamic) {
		bfree(ib->data);
		ib->data = NULL;
	}

	return success;
}

gs_indexbuffer_t *device_indexbuffer_create(gs_device_t *device,
					    enum gs_index_type type,
					    void *indices, size_t num,
					    uint32_t flags)
{
	struct gs_index_buffer *ib = bzalloc(sizeof(struct gs_index_buffer));
	size_t width = type == GS_UNSIGNED_LONG ? 4 : 2;

	ib->device = device;
	ib->data = indices;
	ib->dynamic = flags & GS_DYNAMIC;
	ib->num = num;
	ib->width = width;
	ib->size = width * num;
	ib->type = type;
	ib->gl_type = type == GS_UNSIGNED_LONG ? GL_UNSIGNED_INT
					       : GL_UNSIGNED_SHORT;

	if (!init_ib(ib)) {
		blog(LOG_ERROR, "device_indexbuffer_create (GL) failed");
		gs_indexbuffer_destroy(ib);
		return NULL;
	}

	return ib;
}

void gs_indexbuffer_destroy(gs_indexbuffer_t *ib)
{
	if (ib) {
		if (ib->buffer)
			gl_delete_buffers(1, &ib->buffer);

		bfree(ib->data);
		bfree(ib);
	}
}

static inline void gs_indexbuffer_flush_internal(gs_indexbuffer_t *ib,
						 const void *data)
{
	if (!ib->dynamic) {
		blog(LOG_ERROR, "Index buffer is not dynamic");
		goto fail;
	}

	if (!update_buffer(GL_ELEMENT_ARRAY_BUFFER, ib->buffer, data, ib->size))
		goto fail;

	return;

fail:
	blog(LOG_ERROR, "gs_indexbuffer_flush (GL) failed");
}

void gs_indexbuffer_flush(gs_indexbuffer_t *ib)
{
	gs_indexbuffer_flush_internal(ib, ib->data);
}

void gs_indexbuffer_flush_direct(gs_indexbuffer_t *ib, const void *data)
{
	gs_indexbuffer_flush_internal(ib, data);
}

void *gs_indexbuffer_get_data(const gs_indexbuffer_t *ib)
{
	return ib->data;
}

size_t gs_indexbuffer_get_num_indices(const gs_indexbuffer_t *ib)
{
	return ib->num;
}

enum gs_index_type gs_indexbuffer_get_type(const gs_indexbuffer_t *ib)
{
	return ib->type;
}

void device_load_indexbuffer(gs_device_t *device, gs_indexbuffer_t *ib)
{
	device->cur_index_buffer = ib;
}
