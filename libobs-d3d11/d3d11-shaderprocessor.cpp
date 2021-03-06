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

#include "d3d11-subsystem.hpp"
#include "d3d11-shaderprocessor.hpp"

#include <sstream>
using namespace std;

static const char *semanticInputNames[] = {"POSITION", "NORMAL",   "COLOR",
					   "TANGENT",  "TEXCOORD", "VERTEXID"};
static const char *semanticOutputNames[] = {
	"SV_Position", "NORMAL", "COLOR", "TANGENT", "TEXCOORD", "VERTEXID"};

static const char *ConvertSemanticName(const char *name)
{
	const size_t num = sizeof(semanticInputNames) / sizeof(const char *);
	for (size_t i = 0; i < num; i++) {
		if (strcmp(name, semanticInputNames[i]) == 0)
			return semanticOutputNames[i];
	}

	throw "Unknown Semantic Name";
}

static void GetSemanticInfo(shader_var *var, const char *&name, uint32_t &index)
{
	const char *mapping = var->mapping;
	const char *indexStr = mapping;

	while (*indexStr && !isdigit(*indexStr))
		indexStr++;
	index = (*indexStr) ? strtol(indexStr, NULL, 10) : 0;

	string nameStr;
	nameStr.assign(mapping, indexStr - mapping);
	name = ConvertSemanticName(nameStr.c_str());
}

static void AddInputLayoutVar(shader_var *var,
			      vector<D3D11_INPUT_ELEMENT_DESC> &layout)
{
	D3D11_INPUT_ELEMENT_DESC ied;
	const char *semanticName;
	uint32_t semanticIndex;

	GetSemanticInfo(var, semanticName, semanticIndex);

	memset(&ied, 0, sizeof(ied));
	ied.SemanticName = semanticName;
	ied.SemanticIndex = semanticIndex;
	ied.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

	if (strcmp(var->mapping, "COLOR") == 0) {
		ied.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	} else if (strcmp(var->mapping, "POSITION") == 0 ||
		   strcmp(var->mapping, "NORMAL") == 0 ||
		   strcmp(var->mapping, "TANGENT") == 0) {
		ied.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	} else if (astrcmp_n(var->mapping, "TEXCOORD", 8) == 0) {
		/* type is always a 'float' type */
		switch (var->type[5]) {
		case 0:
			ied.Format = DXGI_FORMAT_R32_FLOAT;
			break;
		case '2':
			ied.Format = DXGI_FORMAT_R32G32_FLOAT;
			break;
		case '3':
		case '4':
			ied.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			break;
		}
	}

	layout.push_back(ied);
}

static inline bool SetSlot(vector<D3D11_INPUT_ELEMENT_DESC> &layout,
			   const char *name, uint32_t index, uint32_t &slotIdx)
{
	for (size_t i = 0; i < layout.size(); i++) {
		D3D11_INPUT_ELEMENT_DESC &input = layout[i];
		if (input.SemanticIndex == index &&
		    strcmpi(input.SemanticName, name) == 0) {
			layout[i].InputSlot = slotIdx++;
			return true;
		}
	}

	return false;
}

static void BuildInputLayoutFromVars(shader_parser *parser, darray *vars,
				     vector<D3D11_INPUT_ELEMENT_DESC> &layout)
{
	shader_var *array = (shader_var *)vars->array;

	for (size_t i = 0; i < vars->num; i++) {
		shader_var *var = array + i;

		if (var->mapping) {
			if (strcmp(var->mapping, "VERTEXID") != 0)
				AddInputLayoutVar(var, layout);
		} else {
			shader_struct *st =
				shader_parser_getstruct(parser, var->type);
			if (st)
				BuildInputLayoutFromVars(parser, &st->vars.da,
							 layout);
		}
	}

	/*
	 * Sets the input slot value for each semantic, however we do it in
	 * a specific order so that it will always match the vertex buffer's
	 * sub-buffer order (points-> normals-> colors-> tangents-> uvcoords)
	 */
	uint32_t slot = 0;
	SetSlot(layout, "SV_Position", 0, slot);
	SetSlot(layout, "NORMAL", 0, slot);
	SetSlot(layout, "COLOR", 0, slot);
	SetSlot(layout, "TANGENT", 0, slot);

	uint32_t index = 0;
	while (SetSlot(layout, "TEXCOORD", index++, slot))
		;
}

void ShaderProcessor::BuildInputLayout(vector<D3D11_INPUT_ELEMENT_DESC> &layout)
{
	shader_func *func = shader_parser_getfunc(&parser, "main");
	if (!func)
		throw "Failed to find 'main' shader function";

	BuildInputLayoutFromVars(&parser, &func->params.da, layout);
}

gs_shader_param::gs_shader_param(shader_var &var, uint32_t &texCounter)
	: name(var.name),
	  type(get_shader_param_type(var.type)),
	  textureID(texCounter),
	  arrayCount(var.array_count),
	  changed(false)
{
	defaultValue.resize(var.default_val.num);
	memcpy(defaultValue.data(), var.default_val.array, var.default_val.num);

	if (type == GS_SHADER_PARAM_TEXTURE)
		texCounter++;
	else
		textureID = 0;
}

static inline void AddParam(shader_var &var, vector<gs_shader_param> &params,
			    uint32_t &texCounter)
{
	if (var.var_type != SHADER_VAR_UNIFORM ||
	    strcmp(var.type, "sampler") == 0)
		return;

	params.push_back(gs_shader_param(var, texCounter));
}

void ShaderProcessor::BuildParams(vector<gs_shader_param> &params)
{
	uint32_t texCounter = 0;

	for (size_t i = 0; i < parser.params.num; i++)
		AddParam(parser.params.array[i], params, texCounter);
}

static inline void AddSampler(gs_device_t *device, shader_sampler &sampler,
			      vector<unique_ptr<ShaderSampler>> &samplers)
{
	gs_sampler_info si;
	shader_sampler_convert(&sampler, &si);
	samplers.emplace_back(new ShaderSampler(sampler.name, device, &si));
}

void ShaderProcessor::BuildSamplers(vector<unique_ptr<ShaderSampler>> &samplers)
{
	for (size_t i = 0; i < parser.samplers.num; i++)
		AddSampler(device, parser.samplers.array[i], samplers);
}

void ShaderProcessor::BuildString(string &outputString)
{
	stringstream output;
	output << "static const bool obs_glsl_compile = false;\n\n";

	cf_token *token = cf_preprocessor_get_tokens(&parser.cfp.pp);
	while (token->type != CFTOKEN_NONE) {
		/* cheaply just replace specific tokens */
		if (strref_cmp(&token->str, "POSITION") == 0)
			output << "SV_Position";
		else if (strref_cmp(&token->str, "TARGET") == 0)
			output << "SV_Target";
		else if (strref_cmp(&token->str, "texture2d") == 0)
			output << "Texture2D";
		else if (strref_cmp(&token->str, "texture3d") == 0)
			output << "Texture3D";
		else if (strref_cmp(&token->str, "texture_cube") == 0)
			output << "TextureCube";
		else if (strref_cmp(&token->str, "texture_rect") == 0)
			throw "texture_rect is not supported in D3D";
		else if (strref_cmp(&token->str, "sampler_state") == 0)
			output << "SamplerState";
		else if (strref_cmp(&token->str, "VERTEXID") == 0)
			output << "SV_VertexID";
		else
			output.write(token->str.array, token->str.len);

		token++;
	}

	outputString = move(output.str());
}

void ShaderProcessor::Process(const char *shader_string, const char *file)
{
	bool success = shader_parse(&parser, shader_string, file);
	char *str = shader_parser_geterrors(&parser);
	if (str) {
		blog(LOG_WARNING, "Shader parser errors/warnings:\n%s\n", str);
		bfree(str);
	}

	if (!success)
		throw "Failed to parse shader";
}
