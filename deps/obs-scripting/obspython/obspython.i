%module(threads="1") obspython
%nothread;
%{
#define SWIG_FILE_WITH_INIT
#define DEPRECATED_START
#define DEPRECATED_END
#include <graphics/graphics.h>
#include <graphics/vec4.h>
#include <graphics/vec3.h>
#include <graphics/vec2.h>
#include <graphics/matrix4.h>
#include <graphics/matrix3.h>
#include <graphics/quat.h>
#include <obs.h>
#include <obs-hotkey.h>
#include <obs-source.h>
#include <obs-data.h>
#include <obs-properties.h>
#include <obs-interaction.h>
#include <callback/calldata.h>
#include <callback/decl.h>
#include <callback/proc.h>
#include <callback/signal.h>
#include <util/bmem.h>
#include <util/base.h>
#include "obs-scripting-config.h"
#include <util/platform.h>

#if UI_ENABLED
#include "obs-frontend-api.h"
#endif

%}

#define DEPRECATED_START
#define DEPRECATED_END
#define EXPORT

%rename(blog) wrap_blog;
%inline %{
static inline void wrap_blog(int log_level, const char *message)
{
        blog(log_level, "%s", message);
}
%}

%include "stdint.i"

/* Used to free when using %newobject functions.  E.G.:
 * %newobject obs_module_get_config_path; */
%typemap(newfree) char * "bfree($1);";

%ignore blog;
%ignore blogva;
%ignore bcrash;
%ignore obs_source_info;
%ignore obs_register_source_s(const struct obs_source_info *info, size_t size);
%ignore obs_output_set_video(obs_output_t *output, video_t *video);
%ignore obs_output_video(const obs_output_t *output);
%ignore obs_add_tick_callback;
%ignore obs_remove_tick_callback;
%ignore obs_add_main_render_callback;
%ignore obs_remove_main_render_callback;
%ignore obs_enum_sources;
%ignore obs_properties_add_button;
%ignore obs_property_set_modified_callback;
%ignore signal_handler_connect;
%ignore signal_handler_disconnect;
%ignore signal_handler_connect_global;
%ignore signal_handler_disconnect_global;
%ignore signal_handler_remove_current;
%ignore obs_hotkey_register_frontend;
%ignore obs_hotkey_register_encoder;
%ignore obs_hotkey_register_output;
%ignore obs_hotkey_register_service;
%ignore obs_hotkey_register_source;
%ignore obs_hotkey_pair_register_frontend;
%ignore obs_hotkey_pair_register_encoder;
%ignore obs_hotkey_pair_register_output;
%ignore obs_hotkey_pair_register_service;
%ignore obs_hotkey_pair_register_source;

/* Typemap for gs_stagesurface_map */
%typemap(arginit, noblock = 1) uint32_t *OUTREF
{
	$*1_type temp$argnum = 0;
	$1 = &temp$argnum;
}

%typemap(argout, noblock = 1, doc = "bytearray") uint32_t *OUTREF
{
}

%typemap(in, numinputs = 0) uint32_t *OUTREF "// typemap(in)";

%typemap(arginit) uint8_t **OUTREF = uint32_t * OUTREF;
%typemap(in) uint8_t **OUTREF = uint32_t * OUTREF;

%typemap(argout, noblock = 1) uint8_t **OUTREF
{
	size_t len = gs_stagesurface_get_height(arg1) * (*arg3);

	const unsigned char *in = temp$argnum;
	const char b64chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	char *out;
	size_t elen;
	size_t i;
	size_t j;
	size_t v;

	elen = len;
	if (len % 3 != 0) {
		elen += 3 - (len % 3);
	}
	elen /= 3;
	elen *= 4;

	out = malloc(elen + 1);
	out[elen] = '\0';

	for (i = 0, j = 0; i < len; i += 3, j += 4) {
		v = in[i];
		v = i + 1 < len ? v << 8 | in[i + 1] : v << 8;
		v = i + 2 < len ? v << 8 | in[i + 2] : v << 8;

		out[j] = b64chars[(v >> 18) & 0x3F];
		out[j + 1] = b64chars[(v >> 12) & 0x3F];
		if (i + 1 < len) {
			out[j + 2] = b64chars[(v >> 6) & 0x3F];
		} else {
			out[j + 2] = '=';
		}
		if (i + 2 < len) {
			out[j + 3] = b64chars[v & 0x3F];
		} else {
			out[j + 3] = '=';
		}
	}

	$result = SWIG_Python_AppendOutput(
		$result,
		PyByteArray_FromStringAndSize((const char *)temp$argnum, len));
	$result = SWIG_Python_AppendOutput(
		$result, SWIG_From_unsigned_SS_int(*arg3));
	$result = SWIG_Python_AppendOutput(
		$result, PyUnicode_FromStringAndSize((const char *)out, elen));
	free(out);
}

bool gs_stagesurface_map(gs_stagesurf_t *stagesurf, uint8_t **OUTREF,
			 uint32_t *OUTREF);

%include "graphics/graphics.h"
%include "graphics/vec4.h"
%include "graphics/vec3.h"
%include "graphics/vec2.h"
%include "graphics/matrix4.h"
%include "graphics/matrix3.h"
%include "graphics/quat.h"
%include "obs-scripting-config.h"
%include "obs-data.h"
%include "obs-source.h"
%include "obs-properties.h"
%include "obs-interaction.h"
%include "obs-hotkey.h"
%include "obs.h"
%include "callback/calldata.h"
%include "callback/proc.h"
%include "callback/signal.h"
%include "util/bmem.h"
%include "util/base.h"
%include "util/platform.h"

#if UI_ENABLED
%include "obs-frontend-api.h"
#endif

/* declare these manually because mutex + GIL = deadlocks */
%thread;
void obs_enter_graphics(void); //Should only block on entering mutex
%nothread;
%include "obs.h"
