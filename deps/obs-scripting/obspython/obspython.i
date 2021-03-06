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
