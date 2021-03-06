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

#pragma once

/*
 *   Parses shaders into GLSL.  Shaders are almost identical to HLSL
 * model 5 so it requires quite a bit of tweaking to convert correctly.
 * Takes the parsed shader data, and builds a GLSL string out of it.
 */

#include <util/dstr.h>
#include <graphics/shader-parser.h>

struct gl_parser_attrib {
	struct dstr name;
	const char *mapping;
	bool input;
};

static inline void gl_parser_attrib_init(struct gl_parser_attrib *attr)
{
	memset(attr, 0, sizeof(struct gl_parser_attrib));
}

static inline void gl_parser_attrib_free(struct gl_parser_attrib *attr)
{
	dstr_free(&attr->name);
}

struct gl_shader_parser {
	enum gs_shader_type type;
	const char *input_prefix;
	const char *output_prefix;
	struct shader_parser parser;
	struct dstr gl_string;
	int sincos_counter;

	DARRAY(uint32_t) texture_samplers;
	DARRAY(struct gl_parser_attrib) attribs;
};

static inline void gl_shader_parser_init(struct gl_shader_parser *glsp,
					 enum gs_shader_type type)
{
	glsp->type = type;

	if (type == GS_SHADER_VERTEX) {
		glsp->input_prefix = "_input_attrib";
		glsp->output_prefix = "_vertex_shader_attrib";
	} else if (type == GS_SHADER_PIXEL) {
		glsp->input_prefix = "_vertex_shader_attrib";
		glsp->output_prefix = "_pixel_shader_attrib";
	}

	shader_parser_init(&glsp->parser);
	dstr_init(&glsp->gl_string);
	da_init(glsp->texture_samplers);
	da_init(glsp->attribs);
	glsp->sincos_counter = 1;
}

static inline void gl_shader_parser_free(struct gl_shader_parser *glsp)
{
	size_t i;
	for (i = 0; i < glsp->attribs.num; i++)
		gl_parser_attrib_free(glsp->attribs.array + i);

	da_free(glsp->attribs);
	da_free(glsp->texture_samplers);
	dstr_free(&glsp->gl_string);
	shader_parser_free(&glsp->parser);
}

extern bool gl_shader_parse(struct gl_shader_parser *glsp,
			    const char *shader_str, const char *file);
