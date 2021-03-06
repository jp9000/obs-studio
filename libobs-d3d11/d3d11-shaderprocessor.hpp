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

#include <graphics/shader-parser.h>

struct ShaderParser : shader_parser {
	inline ShaderParser() { shader_parser_init(this); }
	inline ~ShaderParser() { shader_parser_free(this); }
};

struct ShaderProcessor {
	gs_device_t *device;
	ShaderParser parser;

	void BuildInputLayout(vector<D3D11_INPUT_ELEMENT_DESC> &inputs);
	void BuildParams(vector<gs_shader_param> &params);
	void BuildSamplers(vector<unique_ptr<ShaderSampler>> &samplers);
	void BuildString(string &outputString);
	void Process(const char *shader_string, const char *file);

	inline ShaderProcessor(gs_device_t *device) : device(device) {}
};
