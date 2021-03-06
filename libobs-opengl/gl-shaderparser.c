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
#include "gl-shaderparser.h"

static void gl_write_function_contents(struct gl_shader_parser *glsp,
				       struct cf_token **p_token,
				       const char *end);

static inline struct shader_var *sp_getparam(struct gl_shader_parser *glsp,
					     struct cf_token *token)
{
	size_t i;
	for (i = 0; i < glsp->parser.params.num; i++) {
		struct shader_var *param = glsp->parser.params.array + i;
		if (strref_cmp(&token->str, param->name) == 0)
			return param;
	}

	return NULL;
}

static inline size_t sp_getsampler(struct gl_shader_parser *glsp,
				   struct cf_token *token)
{
	size_t i;
	for (i = 0; i < glsp->parser.samplers.num; i++) {
		struct shader_sampler *sampler =
			glsp->parser.samplers.array + i;
		if (strref_cmp(&token->str, sampler->name) == 0)
			return i;
	}

	return -1;
}

static inline int cmp_type(const char *name, const size_t name_len,
			   const char *type, const size_t type_len)
{
	size_t min_len = (name_len < type_len) ? type_len : name_len;
	return astrcmp_n(name, type, min_len);
}

static bool gl_write_type_n(struct gl_shader_parser *glsp, const char *type,
			    size_t len)
{
	if (cmp_type(type, len, "float2", 6) == 0)
		dstr_cat(&glsp->gl_string, "vec2");
	else if (cmp_type(type, len, "float3", 6) == 0)
		dstr_cat(&glsp->gl_string, "vec3");
	else if (cmp_type(type, len, "float4", 6) == 0)
		dstr_cat(&glsp->gl_string, "vec4");
	else if (cmp_type(type, len, "int2", 4) == 0)
		dstr_cat(&glsp->gl_string, "ivec2");
	else if (cmp_type(type, len, "int3", 4) == 0)
		dstr_cat(&glsp->gl_string, "ivec3");
	else if (cmp_type(type, len, "int4", 4) == 0)
		dstr_cat(&glsp->gl_string, "ivec4");
	else if (cmp_type(type, len, "float3x3", 8) == 0)
		dstr_cat(&glsp->gl_string, "mat3x3");
	else if (cmp_type(type, len, "float3x4", 8) == 0)
		dstr_cat(&glsp->gl_string, "mat3x4");
	else if (cmp_type(type, len, "float4x4", 8) == 0)
		dstr_cat(&glsp->gl_string, "mat4x4");
	else if (cmp_type(type, len, "texture2d", 9) == 0)
		dstr_cat(&glsp->gl_string, "sampler2D");
	else if (cmp_type(type, len, "texture3d", 9) == 0)
		dstr_cat(&glsp->gl_string, "sampler3D");
	else if (cmp_type(type, len, "texture_cube", 12) == 0)
		dstr_cat(&glsp->gl_string, "samplerCube");
	else if (cmp_type(type, len, "texture_rect", 12) == 0)
		dstr_cat(&glsp->gl_string, "sampler2DRect");
	else
		return false;

	return true;
}

static inline void gl_write_type(struct gl_shader_parser *glsp,
				 const char *type)
{
	if (!gl_write_type_n(glsp, type, strlen(type)))
		dstr_cat(&glsp->gl_string, type);
}

static inline bool gl_write_type_token(struct gl_shader_parser *glsp,
				       struct cf_token *token)
{
	return gl_write_type_n(glsp, token->str.array, token->str.len);
}

static void gl_write_var(struct gl_shader_parser *glsp, struct shader_var *var)
{
	if (var->var_type == SHADER_VAR_UNIFORM)
		dstr_cat(&glsp->gl_string, "uniform ");
	else if (var->var_type == SHADER_VAR_CONST)
		dstr_cat(&glsp->gl_string, "const ");
	else if (var->var_type == SHADER_VAR_INOUT)
		dstr_cat(&glsp->gl_string, "inout ");
	else if (var->var_type == SHADER_VAR_OUT)
		dstr_cat(&glsp->gl_string, "out ");

	gl_write_type(glsp, var->type);
	dstr_cat(&glsp->gl_string, " ");
	dstr_cat(&glsp->gl_string, var->name);
}

static inline void gl_write_params(struct gl_shader_parser *glsp)
{
	size_t i;
	for (i = 0; i < glsp->parser.params.num; i++) {
		struct shader_var *var = glsp->parser.params.array + i;
		gl_write_var(glsp, var);
		dstr_cat(&glsp->gl_string, ";\n");
	}

	dstr_cat(&glsp->gl_string, "\n");
}

static void gl_write_storage_var(struct gl_shader_parser *glsp,
				 struct shader_var *var, bool input,
				 const char *prefix);

/* unwraps a structure that's used for input/output */
static void gl_unwrap_storage_struct(struct gl_shader_parser *glsp,
				     struct shader_struct *st, const char *name,
				     bool input, const char *prefix)
{
	struct dstr prefix_str;
	size_t i;

	dstr_init(&prefix_str);
	if (prefix)
		dstr_copy(&prefix_str, prefix);
	dstr_cat(&prefix_str, name);
	dstr_cat(&prefix_str, "_");

	for (i = 0; i < st->vars.num; i++) {
		struct shader_var *st_var = st->vars.array + i;
		gl_write_storage_var(glsp, st_var, input, prefix_str.array);
	}

	dstr_free(&prefix_str);
}

static void gl_write_storage_var(struct gl_shader_parser *glsp,
				 struct shader_var *var, bool input,
				 const char *prefix)
{
	struct shader_struct *st =
		shader_parser_getstruct(&glsp->parser, var->type);

	if (st) {
		gl_unwrap_storage_struct(glsp, st, var->name, input, prefix);
	} else {
		if (input && (strcmp(var->mapping, "VERTEXID") == 0))
			return;
		if (strcmp(var->mapping, "POSITION") == 0) {
			if (!input && (glsp->type == GS_SHADER_VERTEX))
				return;
			if (input && (glsp->type == GS_SHADER_PIXEL))
				return;
		}

		struct gl_parser_attrib attrib;
		gl_parser_attrib_init(&attrib);

		dstr_cat(&glsp->gl_string, input ? "in " : "out ");

		if (prefix)
			dstr_cat(&attrib.name, prefix);
		dstr_cat(&attrib.name, var->name);

		gl_write_type(glsp, var->type);
		dstr_cat(&glsp->gl_string, " ");
		dstr_cat_dstr(&glsp->gl_string, &attrib.name);
		dstr_cat(&glsp->gl_string, ";\n");

		attrib.input = input;
		attrib.mapping = var->mapping;
		da_push_back(glsp->attribs, &attrib);
	}
}

static inline void gl_write_inputs(struct gl_shader_parser *glsp,
				   struct shader_func *main)
{
	size_t i;
	for (i = 0; i < main->params.num; i++)
		gl_write_storage_var(glsp, main->params.array + i, true,
				     "inputval_");
	dstr_cat(&glsp->gl_string, "\n");
}

static void gl_write_outputs(struct gl_shader_parser *glsp,
			     struct shader_func *main)
{
	struct shader_var var = {0};
	var.type = main->return_type;
	var.name = "outputval";
	if (main->mapping)
		var.mapping = main->mapping;

	gl_write_storage_var(glsp, &var, false, NULL);
	dstr_cat(&glsp->gl_string, "\n");
}

static void gl_write_struct(struct gl_shader_parser *glsp,
			    struct shader_struct *st)
{
	size_t i;
	dstr_cat(&glsp->gl_string, "struct ");
	dstr_cat(&glsp->gl_string, st->name);
	dstr_cat(&glsp->gl_string, " {\n");

	for (i = 0; i < st->vars.num; i++) {
		struct shader_var *var = st->vars.array + i;

		dstr_cat(&glsp->gl_string, "\t");
		gl_write_var(glsp, var);
		dstr_cat(&glsp->gl_string, ";\n");
	}

	dstr_cat(&glsp->gl_string, "};\n\n");
}

static void gl_write_interface_block(struct gl_shader_parser *glsp)
{
	if (glsp->type == GS_SHADER_VERTEX) {
		dstr_cat(&glsp->gl_string, "out gl_PerVertex {\n"
					   "\tvec4 gl_Position;\n};\n\n");
	}
}

static inline void gl_write_structs(struct gl_shader_parser *glsp)
{
	size_t i;
	for (i = 0; i < glsp->parser.structs.num; i++) {
		struct shader_struct *st = glsp->parser.structs.array + i;
		gl_write_struct(glsp, st);
	}
}

/*
 * NOTE: HLSL-> GLSL intrinsic conversions
 *   atan2    -> atan
 *   clip     -> (unsupported)
 *   ddx      -> dFdx
 *   ddy      -> dFdy
 *   fmod     -> mod (XXX: these are different if sign is negative)
 *   frac     -> fract
 *   lerp     -> mix
 *   lit      -> (unsupported)
 *   log10    -> (unsupported)
 *   mul      -> (change to operator)
 *   rsqrt    -> inversesqrt
 *   saturate -> (use clamp)
 *   sincos   -> (map to manual sin/cos calls)
 *   tex*     -> texture
 *   tex*grad -> textureGrad
 *   tex*lod  -> textureLod
 *   tex*bias -> (use optional 'bias' value)
 *   tex*proj -> textureProj
 *
 *   All else can be left as-is
 */

static bool gl_write_mul(struct gl_shader_parser *glsp,
			 struct cf_token **p_token)
{
	struct cf_parser *cfp = &glsp->parser.cfp;
	cfp->cur_token = *p_token;

	if (!cf_next_token(cfp))
		return false;
	if (!cf_token_is(cfp, "("))
		return false;

	dstr_cat(&glsp->gl_string, "(");
	gl_write_function_contents(glsp, &cfp->cur_token, ",");
	dstr_cat(&glsp->gl_string, ") * (");
	cf_next_token(cfp);
	gl_write_function_contents(glsp, &cfp->cur_token, ")");
	dstr_cat(&glsp->gl_string, "))");

	*p_token = cfp->cur_token;
	return true;
}

static bool gl_write_sincos(struct gl_shader_parser *glsp,
			    struct cf_token **p_token)
{
	struct cf_parser *cfp = &glsp->parser.cfp;
	struct dstr var = {0};
	bool success = false;

	cfp->cur_token = *p_token;

	if (!cf_next_token(cfp))
		return false;
	if (!cf_token_is(cfp, "("))
		return false;

	dstr_printf(&var, "sincos_var_internal_%d", glsp->sincos_counter++);

	dstr_cat(&glsp->gl_string, "float ");
	dstr_cat_dstr(&glsp->gl_string, &var);
	dstr_cat(&glsp->gl_string, " = ");
	gl_write_function_contents(glsp, &cfp->cur_token, ",");
	dstr_cat(&glsp->gl_string, "); ");

	if (!cf_next_token(cfp))
		goto fail;
	gl_write_function_contents(glsp, &cfp->cur_token, ",");
	dstr_cat(&glsp->gl_string, " = sin(");
	dstr_cat_dstr(&glsp->gl_string, &var);
	dstr_cat(&glsp->gl_string, "); ");

	if (!cf_next_token(cfp))
		goto fail;
	gl_write_function_contents(glsp, &cfp->cur_token, ")");
	dstr_cat(&glsp->gl_string, " = cos(");
	dstr_cat_dstr(&glsp->gl_string, &var);
	dstr_cat(&glsp->gl_string, ")");

	success = true;

fail:
	dstr_free(&var);

	*p_token = cfp->cur_token;
	return success;
}

static bool gl_write_saturate(struct gl_shader_parser *glsp,
			      struct cf_token **p_token)
{
	struct cf_parser *cfp = &glsp->parser.cfp;
	cfp->cur_token = *p_token;

	if (!cf_next_token(cfp))
		return false;
	if (!cf_token_is(cfp, "("))
		return false;

	dstr_cat(&glsp->gl_string, "clamp");
	gl_write_function_contents(glsp, &cfp->cur_token, ")");
	dstr_cat(&glsp->gl_string, ", 0.0, 1.0)");

	*p_token = cfp->cur_token;
	return true;
}

static inline bool gl_write_texture_call(struct gl_shader_parser *glsp,
					 struct shader_var *var,
					 const char *call, bool sampler)
{
	struct cf_parser *cfp = &glsp->parser.cfp;

	if (!cf_next_token(cfp))
		return false;
	if (!cf_token_is(cfp, "("))
		return false;

	if (sampler) {
		if (!cf_next_token(cfp))
			return false;
		const size_t sampler_id = sp_getsampler(glsp, cfp->cur_token);
		if (sampler_id == (size_t)-1)
			return false;
		if (!cf_next_token(cfp))
			return false;
		if (!cf_token_is(cfp, ","))
			return false;

		var->gl_sampler_id = sampler_id;
	}

	dstr_cat(&glsp->gl_string, call);
	dstr_cat(&glsp->gl_string, "(");
	dstr_cat(&glsp->gl_string, var->name);
	dstr_cat(&glsp->gl_string, ", ");
	return true;
}

/* processes texture.Sample(sampler, texcoord) */
static bool gl_write_texture_code(struct gl_shader_parser *glsp,
				  struct cf_token **p_token,
				  struct shader_var *var)
{
	struct cf_parser *cfp = &glsp->parser.cfp;
	bool written = false;
	cfp->cur_token = *p_token;

	if (!cf_next_token(cfp))
		return false;
	if (!cf_token_is(cfp, "."))
		return false;
	if (!cf_next_token(cfp))
		return false;

	const char *function_end = ")";

	if (cf_token_is(cfp, "Sample")) {
		written = gl_write_texture_call(glsp, var, "texture", true);
	} else if (cf_token_is(cfp, "SampleBias")) {
		written = gl_write_texture_call(glsp, var, "texture", true);
	} else if (cf_token_is(cfp, "SampleGrad")) {
		written = gl_write_texture_call(glsp, var, "textureGrad", true);
	} else if (cf_token_is(cfp, "SampleLevel")) {
		written = gl_write_texture_call(glsp, var, "textureLod", true);
	} else if (cf_token_is(cfp, "Load")) {
		const char *const func = (strcmp(var->type, "texture3d") == 0)
						 ? "obs_load_3d"
						 : "obs_load_2d";
		written = gl_write_texture_call(glsp, var, func, false);
	}

	if (!written)
		return false;

	if (!cf_next_token(cfp))
		return false;

	gl_write_function_contents(glsp, &cfp->cur_token, ")");
	dstr_cat(&glsp->gl_string, function_end);

	*p_token = cfp->cur_token;
	return true;
}

static bool gl_write_intrinsic(struct gl_shader_parser *glsp,
			       struct cf_token **p_token)
{
	struct cf_token *token = *p_token;
	bool written = true;

	if (strref_cmp(&token->str, "atan2") == 0) {
		dstr_cat(&glsp->gl_string, "atan");
	} else if (strref_cmp(&token->str, "ddx") == 0) {
		dstr_cat(&glsp->gl_string, "dFdx");
	} else if (strref_cmp(&token->str, "ddy") == 0) {
		dstr_cat(&glsp->gl_string, "dFdy");
	} else if (strref_cmp(&token->str, "frac") == 0) {
		dstr_cat(&glsp->gl_string, "fract");
	} else if (strref_cmp(&token->str, "lerp") == 0) {
		dstr_cat(&glsp->gl_string, "mix");
	} else if (strref_cmp(&token->str, "fmod") == 0) {
		dstr_cat(&glsp->gl_string, "mod");
	} else if (strref_cmp(&token->str, "rsqrt") == 0) {
		dstr_cat(&glsp->gl_string, "inversesqrt");
	} else if (strref_cmp(&token->str, "saturate") == 0) {
		written = gl_write_saturate(glsp, &token);
	} else if (strref_cmp(&token->str, "mul") == 0) {
		written = gl_write_mul(glsp, &token);
	} else if (strref_cmp(&token->str, "sincos") == 0) {
		written = gl_write_sincos(glsp, &token);
	} else {
		struct shader_var *var = sp_getparam(glsp, token);
		if (var && astrcmp_n(var->type, "texture", 7) == 0)
			written = gl_write_texture_code(glsp, &token, var);
		else
			written = false;
	}

	if (written)
		*p_token = token;
	return written;
}

static void gl_write_function_contents(struct gl_shader_parser *glsp,
				       struct cf_token **p_token,
				       const char *end)
{
	struct cf_token *token = *p_token;

	if (token->type != CFTOKEN_NAME || (!gl_write_type_token(glsp, token) &&
					    !gl_write_intrinsic(glsp, &token)))
		dstr_cat_strref(&glsp->gl_string, &token->str);

	while (token->type != CFTOKEN_NONE) {
		token++;

		if (end && strref_cmp(&token->str, end) == 0)
			break;

		if (token->type == CFTOKEN_NAME) {
			if (!gl_write_type_token(glsp, token) &&
			    !gl_write_intrinsic(glsp, &token))
				dstr_cat_strref(&glsp->gl_string, &token->str);

		} else if (token->type == CFTOKEN_OTHER) {
			if (*token->str.array == '{')
				gl_write_function_contents(glsp, &token, "}");
			else if (*token->str.array == '(')
				gl_write_function_contents(glsp, &token, ")");

			dstr_cat_strref(&glsp->gl_string, &token->str);

		} else {
			dstr_cat_strref(&glsp->gl_string, &token->str);
		}
	}

	*p_token = token;
}

static void gl_write_function(struct gl_shader_parser *glsp,
			      struct shader_func *func)
{
	size_t i;
	struct cf_token *token;

	gl_write_type(glsp, func->return_type);
	dstr_cat(&glsp->gl_string, " ");

	if (strcmp(func->name, "main") == 0)
		dstr_cat(&glsp->gl_string, "_main_wrap");
	else
		dstr_cat(&glsp->gl_string, func->name);

	dstr_cat(&glsp->gl_string, "(");

	for (i = 0; i < func->params.num; i++) {
		struct shader_var *param = func->params.array + i;

		if (i > 0)
			dstr_cat(&glsp->gl_string, ", ");
		gl_write_var(glsp, param);
	}

	dstr_cat(&glsp->gl_string, ")\n");

	token = func->start;
	gl_write_function_contents(glsp, &token, "}");
	dstr_cat(&glsp->gl_string, "}\n\n");
}

static inline void gl_write_functions(struct gl_shader_parser *glsp)
{
	size_t i;
	for (i = 0; i < glsp->parser.funcs.num; i++) {
		struct shader_func *func = glsp->parser.funcs.array + i;
		gl_write_function(glsp, func);
	}
}

static inline void gl_write_main_interface_assign(struct gl_shader_parser *glsp,
						  struct shader_var *var,
						  const char *src)
{
	/* vertex shaders: write gl_Position */
	if (glsp->type == GS_SHADER_VERTEX &&
	    strcmp(var->mapping, "POSITION") == 0) {
		dstr_cat(&glsp->gl_string, "\tgl_Position = ");
		dstr_cat(&glsp->gl_string, src);
		dstr_cat(&glsp->gl_string, var->name);
		dstr_cat(&glsp->gl_string, ";\n");
	}
}

static void gl_write_main_storage_assign(struct gl_shader_parser *glsp,
					 struct shader_var *var,
					 const char *dst, const char *src,
					 bool input)
{
	struct shader_struct *st;
	struct dstr dst_copy = {0};
	char ch_left = input ? '.' : '_';
	char ch_right = input ? '_' : '.';

	if (dst) {
		dstr_copy(&dst_copy, dst);
		dstr_cat_ch(&dst_copy, ch_left);
	} else {
		dstr_copy(&dst_copy, "\t");
	}

	dstr_cat(&dst_copy, var->name);

	st = shader_parser_getstruct(&glsp->parser, var->type);
	if (st) {
		struct dstr src_copy = {0};
		size_t i;

		if (src)
			dstr_copy(&src_copy, src);
		dstr_cat(&src_copy, var->name);
		dstr_cat_ch(&src_copy, ch_right);

		for (i = 0; i < st->vars.num; i++) {
			struct shader_var *st_var = st->vars.array + i;
			gl_write_main_storage_assign(glsp, st_var,
						     dst_copy.array,
						     src_copy.array, input);
		}

		dstr_free(&src_copy);
	} else {
		if (input || (glsp->type != GS_SHADER_VERTEX) ||
		    (strcmp(var->mapping, "POSITION"))) {
			if (!dstr_is_empty(&dst_copy))
				dstr_cat_dstr(&glsp->gl_string, &dst_copy);
			dstr_cat(&glsp->gl_string, " = ");
			if (input && (strcmp(var->mapping, "VERTEXID") == 0))
				dstr_cat(&glsp->gl_string, "uint(gl_VertexID)");
			else if (input && (glsp->type == GS_SHADER_PIXEL) &&
				 (strcmp(var->mapping, "POSITION") == 0))
				dstr_cat(&glsp->gl_string, "gl_FragCoord");
			else {
				if (src)
					dstr_cat(&glsp->gl_string, src);
				dstr_cat(&glsp->gl_string, var->name);
			}
			dstr_cat(&glsp->gl_string, ";\n");
		}

		if (!input)
			gl_write_main_interface_assign(glsp, var, src);
	}

	dstr_free(&dst_copy);
}

static inline void gl_write_main_storage_inputs(struct gl_shader_parser *glsp,
						struct shader_func *main)
{
	gl_write_main_storage_assign(glsp, main->params.array, NULL,
				     "inputval_", true);
}

static inline void gl_write_main_storage_outputs(struct gl_shader_parser *glsp,
						 struct shader_func *main)
{
	/* we only do this *if* we're writing a struct, because otherwise
	 * the call to 'main' already does the assignment for us */
	if (!main->mapping) {
		struct shader_var var = {0};
		var.name = "outputval";
		var.type = main->return_type;
		dstr_cat(&glsp->gl_string, "\n");
		gl_write_main_storage_assign(glsp, &var, NULL, NULL, false);
	}
}

static inline void gl_write_main_vars(struct gl_shader_parser *glsp,
				      struct shader_func *main_func)
{
	size_t i;
	for (i = 0; i < main_func->params.num; i++) {
		dstr_cat(&glsp->gl_string, "\t");
		dstr_cat(&glsp->gl_string, main_func->params.array[i].type);
		dstr_cat(&glsp->gl_string, " ");
		dstr_cat(&glsp->gl_string, main_func->params.array[i].name);
		dstr_cat(&glsp->gl_string, ";\n");
	}

	if (!main_func->mapping) {
		dstr_cat(&glsp->gl_string, "\t");
		dstr_cat(&glsp->gl_string, main_func->return_type);
		dstr_cat(&glsp->gl_string, " outputval;\n\n");
	}
}

static inline void gl_write_main_func_call(struct gl_shader_parser *glsp,
					   struct shader_func *main_func)
{
	size_t i;
	dstr_cat(&glsp->gl_string, "\n\toutputval = _main_wrap(");

	for (i = 0; i < main_func->params.num; i++) {
		if (i)
			dstr_cat(&glsp->gl_string, ", ");
		dstr_cat(&glsp->gl_string, main_func->params.array[i].name);
	}

	dstr_cat(&glsp->gl_string, ");\n");
}

static void gl_write_main(struct gl_shader_parser *glsp,
			  struct shader_func *main)
{
	dstr_cat(&glsp->gl_string, "void main(void)\n{\n");

	gl_write_main_vars(glsp, main);
	gl_write_main_storage_inputs(glsp, main);
	gl_write_main_func_call(glsp, main);
	gl_write_main_storage_outputs(glsp, main);

	dstr_cat(&glsp->gl_string, "}\n");
}

/* ugh, don't ask.  I'll probably get rid of the need for this function later */
static void gl_rename_attributes(struct gl_shader_parser *glsp)
{
	size_t i = 0, input_idx = 0, output_idx = 0;

	for (i = 0; i < glsp->attribs.num; i++) {
		struct gl_parser_attrib *attrib = glsp->attribs.array + i;
		struct dstr new_name = {0};
		const char *prefix;
		size_t val;

		if (attrib->input) {
			prefix = glsp->input_prefix;
			val = input_idx++;
		} else {
			prefix = glsp->output_prefix;
			val = output_idx++;
		}

		dstr_printf(&new_name, "%s%u", prefix, (unsigned int)val);
		dstr_replace(&glsp->gl_string, attrib->name.array,
			     new_name.array);
		dstr_move(&attrib->name, &new_name);
	}
}

static bool gl_shader_buildstring(struct gl_shader_parser *glsp)
{
	struct shader_func *main_func;

	main_func = shader_parser_getfunc(&glsp->parser, "main");
	if (!main_func) {
		blog(LOG_ERROR, "function 'main' not found");
		return false;
	}

	dstr_copy(&glsp->gl_string, "#version 330\n\n");
	dstr_cat(&glsp->gl_string, "const bool obs_glsl_compile = true;\n\n");
	dstr_cat(&glsp->gl_string,
		 "vec4 obs_load_2d(sampler2D s, ivec3 p_lod)\n");
	dstr_cat(&glsp->gl_string, "{\n");
	dstr_cat(&glsp->gl_string, "\tint lod = p_lod.z;\n");
	dstr_cat(&glsp->gl_string, "\tvec2 size = textureSize(s, lod);\n");
	dstr_cat(&glsp->gl_string,
		 "\tvec2 p = (vec2(p_lod.xy) + 0.5) / size;\n");
	dstr_cat(&glsp->gl_string, "\tvec4 color = textureLod(s, p, lod);\n");
	dstr_cat(&glsp->gl_string, "\treturn color;\n");
	dstr_cat(&glsp->gl_string, "}\n\n");
	dstr_cat(&glsp->gl_string,
		 "vec4 obs_load_3d(sampler3D s, ivec4 p_lod)\n");
	dstr_cat(&glsp->gl_string, "{\n");
	dstr_cat(&glsp->gl_string, "\tint lod = p_lod.w;\n");
	dstr_cat(&glsp->gl_string, "\tvec3 size = textureSize(s, lod);\n");
	dstr_cat(&glsp->gl_string,
		 "\tvec3 p = (vec3(p_lod.xyz) + 0.5) / size;\n");
	dstr_cat(&glsp->gl_string, "\tvec4 color = textureLod(s, p, lod);\n");
	dstr_cat(&glsp->gl_string, "\treturn color;\n");
	dstr_cat(&glsp->gl_string, "}\n\n");
	gl_write_params(glsp);
	gl_write_inputs(glsp, main_func);
	gl_write_outputs(glsp, main_func);
	gl_write_interface_block(glsp);
	gl_write_structs(glsp);
	gl_write_functions(glsp);
	gl_write_main(glsp, main_func);
	gl_rename_attributes(glsp);

	return true;
}

bool gl_shader_parse(struct gl_shader_parser *glsp, const char *shader_str,
		     const char *file)
{
	bool success = shader_parse(&glsp->parser, shader_str, file);
	char *str = shader_parser_geterrors(&glsp->parser);
	if (str) {
		blog(LOG_WARNING, "Shader parser errors/warnings:\n%s\n", str);
		bfree(str);
	}

	if (success)
		success = gl_shader_buildstring(glsp);

	return success;
}
