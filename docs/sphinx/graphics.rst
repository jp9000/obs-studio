Rendering Graphics
==================

Libobs has a custom-made programmable graphics subsystem that wraps both
Direct3D 11 and OpenGL.  The reason why it was designed with a custom
graphics subsystem was to accommodate custom capture features only
available on specific operating systems.

*(Author's note: In retrospect, I probably should have used something
like ANGLE, but I would have to modify it to accommodate my specific
use-cases.)*

Most rendering is dependent upon effects.  Effects are used by all video
objects in libobs; they're used to easily bundle related vertex/pixel
shaders in to one file.

An effect file has a nearly identical syntax to Direct3D 11 HLSL effect
files.  The only differences are as follows:

- Sampler states are named "sampler_state"
- Position semantic is called "POSITION" rather than "SV_Position"
- Target semantic is called "TARGET" rather than "SV_Target"

*(Author's note: I'm probably missing a few exceptions here, if I am
please let me know)*


The Graphics Context
--------------------

Using graphics functions isn't possible unless the current thread has
entered a graphics context, and the graphics context can only be used by
one thread at a time.  To enter the graphics context, use
:c:func:`obs_enter_graphics()`, and to leave the graphics context, use
:c:func:`obs_leave_graphics()`.

Certain callback will automatically be within the graphics context:
:c:member:`obs_source_info.video_render`, and the draw callback
parameter of :c:func:`obs_display_add_draw_callback()`, and
:c:func:`obs_add_main_render_callback()`.


Creating Effects
----------------

Effect Parameters
^^^^^^^^^^^^^^^^^

To create an effect, it's recommended to start with the uniforms
(parameters) of the effect.

There are a number of different types of uniforms:

+------------------+---------------+------------------+------------+------------+
| Floating points: | **float**     | **float2**       | **float3** | **float4** |
+------------------+---------------+------------------+------------+------------+
| Matrices:        | **float3x3**  | **float4x4**     |            |            |
+------------------+---------------+------------------+------------+------------+
| Integers:        | **int**       | **int2**         | **int3**   | **int4**   |
+------------------+---------------+------------------+------------+------------+
| Booleans:        | **bool**      |                  |            |            |
+------------------+---------------+------------------+------------+------------+
| Textures:        | **texture2d** | **texture_cube** |            |            |
+------------------+---------------+------------------+------------+------------+

To get the effect uniform parameters, you use
:c:func:`gs_effect_get_param_by_name()` or
:c:func:`gs_effect_get_param_by_idx()`.

Then the uniforms are set through the following functions:

- :c:func:`gs_effect_set_bool()`
- :c:func:`gs_effect_set_float()`
- :c:func:`gs_effect_set_int()`
- :c:func:`gs_effect_set_matrix4()`
- :c:func:`gs_effect_set_vec2()`
- :c:func:`gs_effect_set_vec3()`
- :c:func:`gs_effect_set_vec4()`
- :c:func:`gs_effect_set_texture()`
- :c:func:`gs_effect_set_texture_srgb()`

There are two "universal" effect parameters that may be expected of
effects:  **ViewProj**, and **image**.  The **ViewProj** parameter
(which is a float4x4) is used for the primary view/projection matrix
combination.  The **image** parameter (which is a texture2d) is a
commonly used parameter for the main texture; this parameter will be
used with the functions :c:func:`obs_source_draw()`,
:c:func:`gs_draw_sprite()`, and
:c:func:`obs_source_process_filter_end()`.

Here is an example of effect parameters:

.. code:: cpp

   uniform float4x4 ViewProj;
   uniform texture2d image;

   uniform float4 my_color_param;
   uniform float my_float_param;

Effect parameters can also have default values.  Default parameters of
elements that have multiple elements should be treated as an array.

Here are some examples of default parameters:

.. code:: cpp

   uniform float4x4 my_matrix = {1.0, 0.0, 0.0, 0.0,
                                 0.0, 1.0, 0.0, 0.0,
                                 0.0, 0.0, 1.0, 0.0,
                                 0.0, 0.0, 0.0, 1.0};

   uniform float4 my_float4 = {1.0, 0.5, 0.25, 0.0};
   uniform float my_float = 4.0;
   uniform int my_int = 5;

Effect Sampler States
^^^^^^^^^^^^^^^^^^^^^

Then, if textures are used, sampler states should be defined.  Sampler
states have certain sub-parameters:

- **Filter** - The type of filtering to use.  Can be one of the
  following values:

  - **Anisotropy**
  - **Point**
  - **Linear**
  - **MIN_MAG_POINT_MIP_LINEAR**
  - **MIN_POINT_MAG_LINEAR_MIP_POINT**
  - **MIN_POINT_MAG_MIP_LINEAR**
  - **MIN_LINEAR_MAG_MIP_POINT**
  - **MIN_LINEAR_MAG_POINT_MIP_LINEAR**
  - **MIN_MAG_LINEAR_MIP_POINT**

- **AddressU**, **AddressV** -  Specifies how to handle the sampling
  when the coordinate goes beyond 0.0..1.0.  Can be one of the following
  values:

  - **Wrap** or **Repeat**
  - **Clamp** or **None**
  - **Mirror**
  - **Border** (uses *BorderColor* to fill the color)
  - **MirrorOnce**

- **BorderColor** - Specifies the border color if using the "Border"
  address mode.  This value should be a hexadecimal value representing
  the color, in the format of: AARRGGBB.  For example, 7FFF0000 would
  have its alpha value at 127, its red value at 255, and blue and green
  at 0.  If *Border* is not used as an addressing type, this value is
  ignored.

Here is an example of writing a sampler state in an effect file:

.. code:: cpp

   sampler_state defaultSampler {
           Filter      = Linear;
           AddressU    = Border;
           AddressV    = Border;
           BorderColor = 7FFF0000;
   };

This sampler state would use linear filtering, would use border
addressing for texture coordinate values beyond 0.0..1.0, and the border
color would be the color specified above.

When a sampler state is used, it's used identically to the HLSL form:

.. code:: cpp

   [...]

   uniform texture2d image;

   sampler_state defaultSampler {
           Filter      = Linear;
           AddressU    = Clamp;
           AddressV    = Clamp;
   };

   [...]

   float4 MyPixelShaderFunc(VertInOut vert_in) : TARGET
   {
           return image.Sample(def_sampler, vert_in.uv);
   }

Effect Vertex/Pixel Semantics
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Then structures should be defined for inputs and outputs vertex
semantics.

Vertex components can have the following semantics:

- **COLOR**          - Color value (*float4*).
- **POSITION**       - Position value (*float4*).
- **NORMAL**         - Normal value (*float4*).
- **TANGENT**        - Tangent value (*float4*).
- **TEXCOORD[0..7]** - Texture cooordinate value (*float2*, *float3*, or
  *float4*).

Here is an example of a vertex semantic structure:

.. code:: cpp

   struct VertexIn {
           float4 my_position : POSITION;
           float2 my_texcoord : TEXCOORD0;
   };

These semantic structures are then passed in as a parameter to the
primary shader entry point, and used as a return value for the vertex
shader.  Note that the vertex shader is allowed to return different
semantics than it takes in; but the return type of the vertex shader and
the parameter of the pixel shader must match.

The semantic structure used for the parameter to the vertex shader
function will require that the vertex buffer have those values, so if
you have POSITION and TEXCOORD0, the vertex buffer will have to have at
least a position buffer and a texture coordinate buffer in it.

For pixel shaders, they need to return with a TARGET semantic (which is
a float4 RGBA value).  Here is an example of how it's usually used with
a pixel shader function:

.. code:: cpp

   float4 MyPixelShaderFunc(VertInOut vert_in) : TARGET
   {
           return image.Sample(def_sampler, vert_in.uv);
   }


Effect Techniques
^^^^^^^^^^^^^^^^^

Techniques are used to define the primary vertex/pixel shader entry
functions per pass.  One technique can have multiple passes or custom
pass setup.

*(Author's note: These days, multiple passes aren't really needed; GPUs
are powerful enough to where you can perform all actions in the same
shader.  Named passes can be useful for custom draw setups, but even
then you can just make it a separate technique.  For that reason, it's
best to just ignore the extra pass functionality.)*

If you're making an effect filter for video sources, typically you'd
name the pass **Draw**, and then
:c:func:`obs_source_process_filter_end()` will automatically call that
specific effect name.  However, you can also use
:c:func:`obs_source_process_filter_tech_end()` to make the filter use a
specific technique by its name.

The first parameter of the vertex/pixel shader functions in passes
should always be the name of its vertex semantic structure parameter.

For techniques, it's better to show some examples of how techniques
would be used:

.. code:: cpp

   uniform float4x4 ViewProj;
   uniform texture2d image;

   struct VertInOut {
           float4 my_position : POSITION;
           float2 my_texcoord : TEXCOORD0;
   };

   VertInOut MyVertexShaderFunc(VertInOut vert_in)
   {
           VertInOut vert_out;
           vert_out.pos = mul(float4(vert_in.pos.xyz, 1.0), ViewProj);
           vert_out.uv  = vert_in.uv;
           return vert_out;
   }

   float4 MyPixelShaderFunc(VertInOut vert_in) : TARGET
   {
           return image.Sample(def_sampler, vert_in.uv);
   }

   technique Draw
   {
           pass
           {
                   vertex_shader = MyVertexShaderFunc(vert_in);
                   pixel_shader  = MyPixelShaderFunc(vert_in);
           }
   };

Using Effects
-------------

The recommended way to use effects is like so:

.. code:: cpp

   for (gs_effect_loop(effect, "technique")) {
           [draw calls go here]
   }

This will automatically handle loading/unloading of the effect and its
shaders for a given technique name.


Rendering Video Sources
-----------------------

A synchronous video source renders in its
:c:member:`obs_source_info.video_render` callback.

Sources can render with custom drawing (via the OBS_SOURCE_CUSTOM_DRAW
output capability flag), or without.  When sources render without custom
rendering, it's recommended to render a single texture with
:c:func:`obs_source_draw()`.  Otherwise the source is expected to
perform rendering on its own and manage its own effects.

Libobs comes with a set of default/standard effects that can be accessed
via the :c:func:`obs_get_base_effect()` function.  You can use these
effects to render, or you can create custom effects with
:c:func:`gs_effect_create_from_file()` and render with a custom effect.


Rendering Video Effect Filters
------------------------------

For most video effect filters, it comprises of adding a layer of
processing shaders to an existing image in its
:c:member:`obs_source_info.video_render` callback.  When this is the
case, it's expected that the filter has its own effect created, and to
draw the effect, one would simply use the
:c:func:`obs_source_process_filter_begin()` function, set the parameters
on your custom effect, then call either
:c:func:`obs_source_process_filter_end()` or
:c:func:`obs_source_process_filter_tech_end()` to finish rendering the
filter.

Here's an example of rendering a filter from the color key filter:

.. code:: cpp

   static void color_key_render(void *data, gs_effect_t *effect)
   {
           struct color_key_filter_data *filter = data;
   
           if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
                                   OBS_ALLOW_DIRECT_RENDERING))
                   return;
   
           gs_effect_set_vec4(filter->color_param, &filter->color);
           gs_effect_set_float(filter->contrast_param, filter->contrast);
           gs_effect_set_float(filter->brightness_param, filter->brightness);
           gs_effect_set_float(filter->gamma_param, filter->gamma);
           gs_effect_set_vec4(filter->key_color_param, &filter->key_color);
           gs_effect_set_float(filter->similarity_param, filter->similarity);
           gs_effect_set_float(filter->smoothness_param, filter->smoothness);
   
           obs_source_process_filter_end(filter->context, filter->effect, 0, 0);
   
           UNUSED_PARAMETER(effect);
   }

