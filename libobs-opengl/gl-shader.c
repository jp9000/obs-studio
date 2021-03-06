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

#include <assert.h>

#include <graphics/vec2.h>
#include <graphics/vec3.h>
#include <graphics/vec4.h>
#include <graphics/matrix3.h>
#include <graphics/matrix4.h>
#include "gl-subsystem.h"
#include "gl-shaderparser.h"

static inline void shader_param_init(struct gs_shader_param *param)
{
	memset(param, 0, sizeof(struct gs_shader_param));
}

static inline void shader_param_free(struct gs_shader_param *param)
{
	bfree(param->name);
	da_free(param->cur_value);
	da_free(param->def_value);
}

static inline void shader_attrib_free(struct shader_attrib *attrib)
{
	bfree(attrib->name);
}

static void gl_get_shader_info(GLuint shader, const char *file,
			       char **error_string)
{
	char *errors;
	GLint info_len = 0;
	GLsizei chars_written = 0;

	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
	if (!gl_success("glGetProgramiv") || !info_len)
		return;

	errors = bzalloc(info_len + 1);
	glGetShaderInfoLog(shader, info_len, &chars_written, errors);
	gl_success("glGetShaderInfoLog");

	blog(LOG_DEBUG, "Compiler warnings/errors for %s:\n%s", file, errors);

	if (error_string)
		*error_string = errors;
	else
		bfree(errors);
}

static bool gl_add_param(struct gs_shader *shader, struct shader_var *var,
			 GLint *texture_id)
{
	struct gs_shader_param param = {0};

	param.array_count = var->array_count;
	param.name = bstrdup(var->name);
	param.shader = shader;
	param.type = get_shader_param_type(var->type);

	if (param.type == GS_SHADER_PARAM_TEXTURE) {
		param.sampler_id = var->gl_sampler_id;
		param.texture_id = (*texture_id)++;
	} else {
		param.changed = true;
	}

	da_move(param.def_value, var->default_val);
	da_copy(param.cur_value, param.def_value);

	da_push_back(shader->params, &param);
	return true;
}

static inline bool gl_add_params(struct gs_shader *shader,
				 struct gl_shader_parser *glsp)
{
	size_t i;
	GLint tex_id = 0;

	for (i = 0; i < glsp->parser.params.num; i++)
		if (!gl_add_param(shader, glsp->parser.params.array + i,
				  &tex_id))
			return false;

	shader->viewproj = gs_shader_get_param_by_name(shader, "ViewProj");
	shader->world = gs_shader_get_param_by_name(shader, "World");

	return true;
}

static inline void gl_add_sampler(struct gs_shader *shader,
				  struct shader_sampler *sampler)
{
	gs_samplerstate_t *new_sampler;
	struct gs_sampler_info info;

	shader_sampler_convert(sampler, &info);
	new_sampler = device_samplerstate_create(shader->device, &info);

	da_push_back(shader->samplers, &new_sampler);
}

static inline void gl_add_samplers(struct gs_shader *shader,
				   struct gl_shader_parser *glsp)
{
	size_t i;
	for (i = 0; i < glsp->parser.samplers.num; i++) {
		struct shader_sampler *sampler =
			glsp->parser.samplers.array + i;
		gl_add_sampler(shader, sampler);
	}
}

static void get_attrib_type(const char *mapping, enum attrib_type *type,
			    size_t *index)
{
	if (strcmp(mapping, "POSITION") == 0) {
		*type = ATTRIB_POSITION;

	} else if (strcmp(mapping, "NORMAL") == 0) {
		*type = ATTRIB_NORMAL;

	} else if (strcmp(mapping, "TANGENT") == 0) {
		*type = ATTRIB_TANGENT;

	} else if (strcmp(mapping, "COLOR") == 0) {
		*type = ATTRIB_COLOR;

	} else if (astrcmp_n(mapping, "TEXCOORD", 8) == 0) {
		*type = ATTRIB_TEXCOORD;
		*index = (*(mapping + 8)) - '0';
		return;

	} else if (strcmp(mapping, "TARGET") == 0) {
		*type = ATTRIB_TARGET;
	}

	*index = 0;
}

static inline bool gl_process_attrib(struct gs_shader *program,
				     struct gl_parser_attrib *pa)
{
	struct shader_attrib attrib = {0};

	/* don't parse output attributes */
	if (!pa->input)
		return true;

	get_attrib_type(pa->mapping, &attrib.type, &attrib.index);
	attrib.name = pa->name.array;

	pa->name.array = NULL;
	pa->name.len = 0;
	pa->name.capacity = 0;

	da_push_back(program->attribs, &attrib);
	return true;
}

static inline bool gl_process_attribs(struct gs_shader *shader,
				      struct gl_shader_parser *glsp)
{
	size_t i;
	for (i = 0; i < glsp->attribs.num; i++) {
		struct gl_parser_attrib *pa = glsp->attribs.array + i;
		if (!gl_process_attrib(shader, pa))
			return false;
	}

	return true;
}

static bool gl_shader_init(struct gs_shader *shader,
			   struct gl_shader_parser *glsp, const char *file,
			   char **error_string)
{
	GLenum type = convert_shader_type(shader->type);
	int compiled = 0;
	bool success = true;

	shader->obj = glCreateShader(type);
	if (!gl_success("glCreateShader") || !shader->obj)
		return false;

	glShaderSource(shader->obj, 1, (const GLchar **)&glsp->gl_string.array,
		       0);
	if (!gl_success("glShaderSource"))
		return false;

	glCompileShader(shader->obj);
	if (!gl_success("glCompileShader"))
		return false;

#if 0
	blog(LOG_DEBUG, "+++++++++++++++++++++++++++++++++++");
	blog(LOG_DEBUG, "  GL shader string for: %s", file);
	blog(LOG_DEBUG, "-----------------------------------");
	blog(LOG_DEBUG, "%s", glsp->gl_string.array);
	blog(LOG_DEBUG, "+++++++++++++++++++++++++++++++++++");
#endif

	glGetShaderiv(shader->obj, GL_COMPILE_STATUS, &compiled);
	if (!gl_success("glGetShaderiv"))
		return false;

	if (!compiled) {
		GLint infoLength = 0;
		glGetShaderiv(shader->obj, GL_INFO_LOG_LENGTH, &infoLength);

		char *infoLog = malloc(sizeof(char) * infoLength);

		GLsizei returnedLength = 0;
		glGetShaderInfoLog(shader->obj, infoLength, &returnedLength,
				   infoLog);
		blog(LOG_ERROR, "Error compiling shader:\n%s\n", infoLog);

		free(infoLog);

		success = false;
	}

	gl_get_shader_info(shader->obj, file, error_string);

	if (success)
		success = gl_add_params(shader, glsp);
	/* Only vertex shaders actually require input attributes */
	if (success && shader->type == GS_SHADER_VERTEX)
		success = gl_process_attribs(shader, glsp);
	if (success)
		gl_add_samplers(shader, glsp);

	return success;
}

static struct gs_shader *shader_create(gs_device_t *device,
				       enum gs_shader_type type,
				       const char *shader_str, const char *file,
				       char **error_string)
{
	struct gs_shader *shader = bzalloc(sizeof(struct gs_shader));
	struct gl_shader_parser glsp;
	bool success = true;

	shader->device = device;
	shader->type = type;

	gl_shader_parser_init(&glsp, type);
	if (!gl_shader_parse(&glsp, shader_str, file))
		success = false;
	else
		success = gl_shader_init(shader, &glsp, file, error_string);

	if (!success) {
		gs_shader_destroy(shader);
		shader = NULL;
	}

	gl_shader_parser_free(&glsp);
	return shader;
}

gs_shader_t *device_vertexshader_create(gs_device_t *device, const char *shader,
					const char *file, char **error_string)
{
	struct gs_shader *ptr;
	ptr = shader_create(device, GS_SHADER_VERTEX, shader, file,
			    error_string);
	if (!ptr)
		blog(LOG_ERROR, "device_vertexshader_create (GL) failed");
	return ptr;
}

gs_shader_t *device_pixelshader_create(gs_device_t *device, const char *shader,
				       const char *file, char **error_string)
{
	struct gs_shader *ptr;
	ptr = shader_create(device, GS_SHADER_PIXEL, shader, file,
			    error_string);
	if (!ptr)
		blog(LOG_ERROR, "device_pixelshader_create (GL) failed");
	return ptr;
}

static void remove_program_references(struct gs_shader *shader)
{
	struct gs_program *program = shader->device->first_program;

	while (program) {
		struct gs_program *next = program->next;
		bool destroy = false;

		if (shader->type == GS_SHADER_VERTEX &&
		    program->vertex_shader == shader)
			destroy = true;

		else if (shader->type == GS_SHADER_PIXEL &&
			 program->pixel_shader == shader)
			destroy = true;

		if (destroy)
			gs_program_destroy(program);

		program = next;
	}
}

void gs_shader_destroy(gs_shader_t *shader)
{
	size_t i;

	if (!shader)
		return;

	remove_program_references(shader);

	for (i = 0; i < shader->attribs.num; i++)
		shader_attrib_free(shader->attribs.array + i);

	for (i = 0; i < shader->samplers.num; i++)
		gs_samplerstate_destroy(shader->samplers.array[i]);

	for (i = 0; i < shader->params.num; i++)
		shader_param_free(shader->params.array + i);

	if (shader->obj) {
		glDeleteShader(shader->obj);
		gl_success("glDeleteShader");
	}

	da_free(shader->samplers);
	da_free(shader->params);
	da_free(shader->attribs);
	bfree(shader);
}

int gs_shader_get_num_params(const gs_shader_t *shader)
{
	return (int)shader->params.num;
}

gs_sparam_t *gs_shader_get_param_by_idx(gs_shader_t *shader, uint32_t param)
{
	assert(param < shader->params.num);
	return shader->params.array + param;
}

gs_sparam_t *gs_shader_get_param_by_name(gs_shader_t *shader, const char *name)
{
	size_t i;
	for (i = 0; i < shader->params.num; i++) {
		struct gs_shader_param *param = shader->params.array + i;

		if (strcmp(param->name, name) == 0)
			return param;
	}

	return NULL;
}

gs_sparam_t *gs_shader_get_viewproj_matrix(const gs_shader_t *shader)
{
	return shader->viewproj;
}

gs_sparam_t *gs_shader_get_world_matrix(const gs_shader_t *shader)
{
	return shader->world;
}

void gs_shader_get_param_info(const gs_sparam_t *param,
			      struct gs_shader_param_info *info)
{
	info->type = param->type;
	info->name = param->name;
}

void gs_shader_set_bool(gs_sparam_t *param, bool val)
{
	int int_val = val;
	da_copy_array(param->cur_value, &int_val, sizeof(int_val));
}

void gs_shader_set_float(gs_sparam_t *param, float val)
{
	da_copy_array(param->cur_value, &val, sizeof(val));
}

void gs_shader_set_int(gs_sparam_t *param, int val)
{
	da_copy_array(param->cur_value, &val, sizeof(val));
}

void gs_shader_set_matrix3(gs_sparam_t *param, const struct matrix3 *val)
{
	struct matrix4 mat;
	matrix4_from_matrix3(&mat, val);

	da_copy_array(param->cur_value, &mat, sizeof(mat));
}

void gs_shader_set_matrix4(gs_sparam_t *param, const struct matrix4 *val)
{
	da_copy_array(param->cur_value, val, sizeof(*val));
}

void gs_shader_set_vec2(gs_sparam_t *param, const struct vec2 *val)
{
	da_copy_array(param->cur_value, val->ptr, sizeof(*val));
}

void gs_shader_set_vec3(gs_sparam_t *param, const struct vec3 *val)
{
	da_copy_array(param->cur_value, val->ptr, sizeof(*val));
}

void gs_shader_set_vec4(gs_sparam_t *param, const struct vec4 *val)
{
	da_copy_array(param->cur_value, val->ptr, sizeof(*val));
}

void gs_shader_set_texture(gs_sparam_t *param, gs_texture_t *val)
{
	param->texture = val;
}

static inline bool validate_param(struct program_param *pp,
				  size_t expected_size)
{
	if (pp->param->cur_value.num != expected_size) {
		blog(LOG_ERROR,
		     "Parameter '%s' set to invalid size %u, "
		     "expected %u",
		     pp->param->name, (unsigned int)pp->param->cur_value.num,
		     (unsigned int)expected_size);
		return false;
	}

	return true;
}

static void program_set_param_data(struct gs_program *program,
				   struct program_param *pp)
{
	void *array = pp->param->cur_value.array;

	if (pp->param->type == GS_SHADER_PARAM_BOOL ||
	    pp->param->type == GS_SHADER_PARAM_INT) {
		if (validate_param(pp, sizeof(int))) {
			glUniform1iv(pp->obj, 1, (int *)array);
			gl_success("glUniform1iv");
		}

	} else if (pp->param->type == GS_SHADER_PARAM_INT2) {
		if (validate_param(pp, sizeof(int) * 2)) {
			glUniform2iv(pp->obj, 1, (int *)array);
			gl_success("glUniform2iv");
		}

	} else if (pp->param->type == GS_SHADER_PARAM_INT3) {
		if (validate_param(pp, sizeof(int) * 3)) {
			glUniform3iv(pp->obj, 1, (int *)array);
			gl_success("glUniform3iv");
		}

	} else if (pp->param->type == GS_SHADER_PARAM_INT4) {
		if (validate_param(pp, sizeof(int) * 4)) {
			glUniform4iv(pp->obj, 1, (int *)array);
			gl_success("glUniform4iv");
		}

	} else if (pp->param->type == GS_SHADER_PARAM_FLOAT) {
		if (validate_param(pp, sizeof(float))) {
			glUniform1fv(pp->obj, 1, (float *)array);
			gl_success("glUniform1fv");
		}

	} else if (pp->param->type == GS_SHADER_PARAM_VEC2) {
		if (validate_param(pp, sizeof(struct vec2))) {
			glUniform2fv(pp->obj, 1, (float *)array);
			gl_success("glUniform2fv");
		}

	} else if (pp->param->type == GS_SHADER_PARAM_VEC3) {
		if (validate_param(pp, sizeof(float) * 3)) {
			glUniform3fv(pp->obj, 1, (float *)array);
			gl_success("glUniform3fv");
		}

	} else if (pp->param->type == GS_SHADER_PARAM_VEC4) {
		if (validate_param(pp, sizeof(struct vec4))) {
			glUniform4fv(pp->obj, 1, (float *)array);
			gl_success("glUniform4fv");
		}

	} else if (pp->param->type == GS_SHADER_PARAM_MATRIX4X4) {
		if (validate_param(pp, sizeof(struct matrix4))) {
			glUniformMatrix4fv(pp->obj, 1, false, (float *)array);
			gl_success("glUniformMatrix4fv");
		}

	} else if (pp->param->type == GS_SHADER_PARAM_TEXTURE) {
		if (pp->param->next_sampler) {
			program->device->cur_samplers[pp->param->sampler_id] =
				pp->param->next_sampler;
			pp->param->next_sampler = NULL;
		}

		glUniform1i(pp->obj, pp->param->texture_id);
		if (pp->param->srgb)
			device_load_texture_srgb(program->device,
						 pp->param->texture,
						 pp->param->texture_id);
		else
			device_load_texture(program->device, pp->param->texture,
					    pp->param->texture_id);
	}
}

void program_update_params(struct gs_program *program)
{
	for (size_t i = 0; i < program->params.num; i++) {
		struct program_param *pp = program->params.array + i;
		program_set_param_data(program, pp);
	}
}

static void print_link_errors(GLuint program)
{
	char *errors = NULL;
	GLint info_len = 0;
	GLsizei chars_written = 0;

	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
	if (!gl_success("glGetProgramiv") || !info_len)
		return;

	errors = calloc(1, info_len + 1);
	glGetProgramInfoLog(program, info_len, &chars_written, errors);
	gl_success("glGetShaderInfoLog");

	blog(LOG_DEBUG, "Linker warnings/errors:\n%s", errors);

	free(errors);
}

static bool assign_program_attrib(struct gs_program *program,
				  struct shader_attrib *attrib)
{
	GLint attrib_obj = glGetAttribLocation(program->obj, attrib->name);
	if (!gl_success("glGetAttribLocation"))
		return false;

	if (attrib_obj == -1) {
		blog(LOG_ERROR,
		     "glGetAttribLocation: Could not find "
		     "attribute '%s'",
		     attrib->name);
		return false;
	}

	da_push_back(program->attribs, &attrib_obj);
	return true;
}

static inline bool assign_program_attribs(struct gs_program *program)
{
	struct gs_shader *shader = program->vertex_shader;

	for (size_t i = 0; i < shader->attribs.num; i++) {
		struct shader_attrib *attrib = shader->attribs.array + i;
		if (!assign_program_attrib(program, attrib))
			return false;
	}

	return true;
}

static bool assign_program_param(struct gs_program *program,
				 struct gs_shader_param *param)
{
	struct program_param info;

	info.obj = glGetUniformLocation(program->obj, param->name);
	if (!gl_success("glGetUniformLocation"))
		return false;

	if (info.obj == -1) {
		return true;
	}

	info.param = param;
	da_push_back(program->params, &info);
	return true;
}

static inline bool assign_program_shader_params(struct gs_program *program,
						struct gs_shader *shader)
{
	for (size_t i = 0; i < shader->params.num; i++) {
		struct gs_shader_param *param = shader->params.array + i;
		if (!assign_program_param(program, param))
			return false;
	}

	return true;
}

static inline bool assign_program_params(struct gs_program *program)
{
	if (!assign_program_shader_params(program, program->vertex_shader))
		return false;
	if (!assign_program_shader_params(program, program->pixel_shader))
		return false;

	return true;
}

struct gs_program *gs_program_create(struct gs_device *device)
{
	struct gs_program *program = bzalloc(sizeof(*program));
	int linked = false;

	program->device = device;
	program->vertex_shader = device->cur_vertex_shader;
	program->pixel_shader = device->cur_pixel_shader;

	program->obj = glCreateProgram();
	if (!gl_success("glCreateProgram"))
		goto error_detach_neither;

	glAttachShader(program->obj, program->vertex_shader->obj);
	if (!gl_success("glAttachShader (vertex)"))
		goto error_detach_neither;

	glAttachShader(program->obj, program->pixel_shader->obj);
	if (!gl_success("glAttachShader (pixel)"))
		goto error_detach_vertex;

	glLinkProgram(program->obj);
	if (!gl_success("glLinkProgram"))
		goto error;

	glGetProgramiv(program->obj, GL_LINK_STATUS, &linked);
	if (!gl_success("glGetProgramiv"))
		goto error;

	if (linked == GL_FALSE) {
		print_link_errors(program->obj);
		goto error;
	}

	if (!assign_program_attribs(program))
		goto error;
	if (!assign_program_params(program))
		goto error;

	glDetachShader(program->obj, program->vertex_shader->obj);
	gl_success("glDetachShader (vertex)");

	glDetachShader(program->obj, program->pixel_shader->obj);
	gl_success("glDetachShader (pixel)");

	program->next = device->first_program;
	program->prev_next = &device->first_program;
	device->first_program = program;
	if (program->next)
		program->next->prev_next = &program->next;

	return program;

error:
	glDetachShader(program->obj, program->pixel_shader->obj);
	gl_success("glDetachShader (pixel)");

error_detach_vertex:
	glDetachShader(program->obj, program->vertex_shader->obj);
	gl_success("glDetachShader (vertex)");

error_detach_neither:
	gs_program_destroy(program);
	return NULL;
}

void gs_program_destroy(struct gs_program *program)
{
	if (!program)
		return;

	if (program->device->cur_program == program) {
		program->device->cur_program = 0;
		glUseProgram(0);
		gl_success("glUseProgram (zero)");
	}

	da_free(program->attribs);
	da_free(program->params);

	if (program->next)
		program->next->prev_next = program->prev_next;
	if (program->prev_next)
		*program->prev_next = program->next;

	glDeleteProgram(program->obj);
	gl_success("glDeleteProgram");

	bfree(program);
}

void gs_shader_set_val(gs_sparam_t *param, const void *val, size_t size)
{
	int count = param->array_count;
	size_t expected_size = 0;
	if (!count)
		count = 1;

	switch ((uint32_t)param->type) {
	case GS_SHADER_PARAM_FLOAT:
		expected_size = sizeof(float);
		break;
	case GS_SHADER_PARAM_BOOL:
	case GS_SHADER_PARAM_INT:
		expected_size = sizeof(int);
		break;
	case GS_SHADER_PARAM_INT2:
		expected_size = sizeof(int) * 2;
		break;
	case GS_SHADER_PARAM_INT3:
		expected_size = sizeof(int) * 3;
		break;
	case GS_SHADER_PARAM_INT4:
		expected_size = sizeof(int) * 4;
		break;
	case GS_SHADER_PARAM_VEC2:
		expected_size = sizeof(float) * 2;
		break;
	case GS_SHADER_PARAM_VEC3:
		expected_size = sizeof(float) * 3;
		break;
	case GS_SHADER_PARAM_VEC4:
		expected_size = sizeof(float) * 4;
		break;
	case GS_SHADER_PARAM_MATRIX4X4:
		expected_size = sizeof(float) * 4 * 4;
		break;
	case GS_SHADER_PARAM_TEXTURE:
		expected_size = sizeof(struct gs_shader_texture);
		break;
	default:
		expected_size = 0;
	}

	expected_size *= count;
	if (!expected_size)
		return;

	if (expected_size != size) {
		blog(LOG_ERROR, "gs_shader_set_val (GL): Size of shader "
				"param does not match the size of the input");
		return;
	}

	if (param->type == GS_SHADER_PARAM_TEXTURE) {
		struct gs_shader_texture shader_tex;
		memcpy(&shader_tex, val, sizeof(shader_tex));
		gs_shader_set_texture(param, shader_tex.tex);
		param->srgb = shader_tex.srgb;
	} else {
		da_copy_array(param->cur_value, val, size);
	}
}

void gs_shader_set_default(gs_sparam_t *param)
{
	gs_shader_set_val(param, param->def_value.array, param->def_value.num);
}

void gs_shader_set_next_sampler(gs_sparam_t *param, gs_samplerstate_t *sampler)
{
	param->next_sampler = sampler;
}
