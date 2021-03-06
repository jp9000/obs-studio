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

#include <graphics/vec3.h>
#include "gl-subsystem.h"

static bool create_buffers(struct gs_vertex_buffer *vb)
{
	GLenum usage = vb->dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW;
	size_t i;

	if (!gl_create_buffer(GL_ARRAY_BUFFER, &vb->vertex_buffer,
			      vb->data->num * sizeof(struct vec3),
			      vb->data->points, usage))
		return false;

	if (vb->data->normals) {
		if (!gl_create_buffer(GL_ARRAY_BUFFER, &vb->normal_buffer,
				      vb->data->num * sizeof(struct vec3),
				      vb->data->normals, usage))
			return false;
	}

	if (vb->data->tangents) {
		if (!gl_create_buffer(GL_ARRAY_BUFFER, &vb->tangent_buffer,
				      vb->data->num * sizeof(struct vec3),
				      vb->data->tangents, usage))
			return false;
	}

	if (vb->data->colors) {
		if (!gl_create_buffer(GL_ARRAY_BUFFER, &vb->color_buffer,
				      vb->data->num * sizeof(uint32_t),
				      vb->data->colors, usage))
			return false;
	}

	da_reserve(vb->uv_buffers, vb->data->num_tex);
	da_reserve(vb->uv_sizes, vb->data->num_tex);

	for (i = 0; i < vb->data->num_tex; i++) {
		GLuint tex_buffer;
		struct gs_tvertarray *tv = vb->data->tvarray + i;
		size_t size = vb->data->num * sizeof(float) * tv->width;

		if (!gl_create_buffer(GL_ARRAY_BUFFER, &tex_buffer, size,
				      tv->array, usage))
			return false;

		da_push_back(vb->uv_buffers, &tex_buffer);
		da_push_back(vb->uv_sizes, &tv->width);
	}

	if (!vb->dynamic) {
		gs_vbdata_destroy(vb->data);
		vb->data = NULL;
	}

	if (!gl_gen_vertex_arrays(1, &vb->vao))
		return false;

	return true;
}

gs_vertbuffer_t *device_vertexbuffer_create(gs_device_t *device,
					    struct gs_vb_data *data,
					    uint32_t flags)
{
	struct gs_vertex_buffer *vb = bzalloc(sizeof(struct gs_vertex_buffer));
	vb->device = device;
	vb->data = data;
	vb->num = data->num;
	vb->dynamic = flags & GS_DYNAMIC;

	if (!create_buffers(vb)) {
		blog(LOG_ERROR, "device_vertexbuffer_create (GL) failed");
		gs_vertexbuffer_destroy(vb);
		return NULL;
	}

	return vb;
}

void gs_vertexbuffer_destroy(gs_vertbuffer_t *vb)
{
	if (vb) {
		if (vb->vertex_buffer)
			gl_delete_buffers(1, &vb->vertex_buffer);
		if (vb->normal_buffer)
			gl_delete_buffers(1, &vb->normal_buffer);
		if (vb->tangent_buffer)
			gl_delete_buffers(1, &vb->tangent_buffer);
		if (vb->color_buffer)
			gl_delete_buffers(1, &vb->color_buffer);
		if (vb->uv_buffers.num)
			gl_delete_buffers((GLsizei)vb->uv_buffers.num,
					  vb->uv_buffers.array);

		if (vb->vao)
			gl_delete_vertex_arrays(1, &vb->vao);

		da_free(vb->uv_sizes);
		da_free(vb->uv_buffers);
		gs_vbdata_destroy(vb->data);

		bfree(vb);
	}
}

static inline void gs_vertexbuffer_flush_internal(gs_vertbuffer_t *vb,
						  const struct gs_vb_data *data)
{
	size_t i;
	size_t num_tex = data->num_tex < vb->data->num_tex ? data->num_tex
							   : vb->data->num_tex;

	if (!vb->dynamic) {
		blog(LOG_ERROR, "vertex buffer is not dynamic");
		goto failed;
	}

	if (data->points) {
		if (!update_buffer(GL_ARRAY_BUFFER, vb->vertex_buffer,
				   data->points,
				   data->num * sizeof(struct vec3)))
			goto failed;
	}

	if (vb->normal_buffer && data->normals) {
		if (!update_buffer(GL_ARRAY_BUFFER, vb->normal_buffer,
				   data->normals,
				   data->num * sizeof(struct vec3)))
			goto failed;
	}

	if (vb->tangent_buffer && data->tangents) {
		if (!update_buffer(GL_ARRAY_BUFFER, vb->tangent_buffer,
				   data->tangents,
				   data->num * sizeof(struct vec3)))
			goto failed;
	}

	if (vb->color_buffer && data->colors) {
		if (!update_buffer(GL_ARRAY_BUFFER, vb->color_buffer,
				   data->colors, data->num * sizeof(uint32_t)))
			goto failed;
	}

	for (i = 0; i < num_tex; i++) {
		GLuint buffer = vb->uv_buffers.array[i];
		struct gs_tvertarray *tv = data->tvarray + i;
		size_t size = data->num * tv->width * sizeof(float);

		if (!update_buffer(GL_ARRAY_BUFFER, buffer, tv->array, size))
			goto failed;
	}

	return;

failed:
	blog(LOG_ERROR, "gs_vertexbuffer_flush (GL) failed");
}

void gs_vertexbuffer_flush(gs_vertbuffer_t *vb)
{
	gs_vertexbuffer_flush_internal(vb, vb->data);
}

void gs_vertexbuffer_flush_direct(gs_vertbuffer_t *vb,
				  const struct gs_vb_data *data)
{
	gs_vertexbuffer_flush_internal(vb, data);
}

struct gs_vb_data *gs_vertexbuffer_get_data(const gs_vertbuffer_t *vb)
{
	return vb->data;
}

static inline GLuint get_vb_buffer(struct gs_vertex_buffer *vb,
				   enum attrib_type type, size_t index,
				   GLint *width, GLenum *gl_type)
{
	*gl_type = GL_FLOAT;
	*width = 4;

	if (type == ATTRIB_POSITION) {
		return vb->vertex_buffer;
	} else if (type == ATTRIB_NORMAL) {
		return vb->normal_buffer;
	} else if (type == ATTRIB_TANGENT) {
		return vb->tangent_buffer;
	} else if (type == ATTRIB_COLOR) {
		*gl_type = GL_UNSIGNED_BYTE;
		return vb->color_buffer;
	} else if (type == ATTRIB_TEXCOORD) {
		if (vb->uv_buffers.num <= index)
			return 0;

		*width = (GLint)vb->uv_sizes.array[index];
		return vb->uv_buffers.array[index];
	}

	return 0;
}

static bool load_vb_buffer(struct shader_attrib *attrib,
			   struct gs_vertex_buffer *vb, GLint id)
{
	GLenum type;
	GLint width;
	GLuint buffer;
	bool success = true;

	buffer = get_vb_buffer(vb, attrib->type, attrib->index, &width, &type);
	if (!buffer) {
		blog(LOG_ERROR, "Vertex buffer does not have the required "
				"inputs for vertex shader");
		return false;
	}

	if (!gl_bind_buffer(GL_ARRAY_BUFFER, buffer))
		return false;

	glVertexAttribPointer(id, width, type, GL_TRUE, 0, 0);
	if (!gl_success("glVertexAttribPointer"))
		success = false;

	glEnableVertexAttribArray(id);
	if (!gl_success("glEnableVertexAttribArray"))
		success = false;

	if (!gl_bind_buffer(GL_ARRAY_BUFFER, 0))
		success = false;

	return success;
}

bool load_vb_buffers(struct gs_program *program, struct gs_vertex_buffer *vb,
		     struct gs_index_buffer *ib)
{
	struct gs_shader *shader = program->vertex_shader;
	size_t i;

	if (!gl_bind_vertex_array(vb->vao))
		return false;

	for (i = 0; i < shader->attribs.num; i++) {
		struct shader_attrib *attrib = shader->attribs.array + i;
		if (!load_vb_buffer(attrib, vb, program->attribs.array[i]))
			return false;
	}

	if (ib && !gl_bind_buffer(GL_ELEMENT_ARRAY_BUFFER, ib->buffer))
		return false;

	return true;
}

void device_load_vertexbuffer(gs_device_t *device, gs_vertbuffer_t *vb)
{
	device->cur_vertex_buffer = vb;
}
