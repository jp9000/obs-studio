Effects (Shaders)
=================

Effects are a single collection of related shaders.  They're used for
easily writing vertex and pixel shaders together all in the same file in
HLSL format.

.. code:: cpp

   #include <graphics/graphics.h>

.. type:: typedef struct gs_effect           gs_effect_t

   Effect object.

.. type:: typedef struct gs_effect_technique gs_technique_t

   Technique object.

.. type:: typedef struct gs_effect_param     gs_eparam_t

   Effect parameter object.


---------------------

.. function:: gs_effect_t *gs_effect_create_from_file(const char *file, char **error_string)

   Creates an effect from file.

   :param file:         Path to the effect file
   :param error_string: Receives a pointer to the error string, which
                        must be freed with :c:func:`bfree()`.  If
                        *NULL*, this parameter is ignored.
   :return:             The effect object, or *NULL* on error

---------------------

.. function:: gs_effect_t *gs_effect_create(const char *effect_string, const char *filename, char **error_string)

   Creates an effect from a string.

   :param effect_String: Effect string
   :param error_string:  Receives a pointer to the error string, which
                         must be freed with :c:func:`bfree()`.  If
                         *NULL*, this parameter is ignored.
   :return:              The effect object, or *NULL* on error

---------------------

.. function:: void gs_effect_destroy(gs_effect_t *effect)

   Destroys the effect

   :param effect: Effect object

---------------------

.. function:: gs_technique_t *gs_effect_get_technique(const gs_effect_t *effect, const char *name)

   Gets a technique of the effect.

   :param effect: Effect object
   :param name:   Name of the technique
   :return:       Technique object, or *NULL* if not found

---------------------

.. function:: gs_technique_t *gs_effect_get_current_technique(const gs_effect_t *effect)

   Gets the current active technique of the effect.

   :param effect: Effect object
   :return:       Technique object, or *NULL* if none currently active

---------------------

.. function:: size_t gs_technique_begin(gs_technique_t *technique)

   Begins a technique.

   :param technique: Technique object
   :return:          Number of passes this technique uses

---------------------

.. function:: void gs_technique_end(gs_technique_t *technique)

   Ends a technique.  Make sure all active passes have been ended before
   calling.

   :param technique: Technique object

---------------------

.. function:: bool gs_technique_begin_pass(gs_technique_t *technique, size_t pass)

   Begins a pass.  Automatically loads the vertex/pixel shaders
   associated with this pass.  Draw after calling this function.

   :param technique: Technique object
   :param pass:      Pass index
   :return:          *true* if the pass is valid, *false* otherwise

---------------------

.. function:: bool gs_technique_begin_pass_by_name(gs_technique_t *technique, const char *name)

   Begins a pass by its name if the pass has a name.  Automatically
   loads the vertex/pixel shaders associated with this pass.  Draw after
   calling this function.

   :param technique: Technique object
   :param name:      Name of the pass
   :return:          *true* if the pass is valid, *false* otherwise

---------------------

.. function:: void gs_technique_end_pass(gs_technique_t *technique)

   Ends a pass.

   :param technique: Technique object

---------------------

.. function:: size_t gs_effect_get_num_params(const gs_effect_t *effect)

   Gets the number of parameters associated with the effect.

   :param effect: Effect object
   :return:       Number of parameters the effect has

---------------------

.. function:: gs_eparam_t *gs_effect_get_param_by_idx(const gs_effect_t *effect, size_t param)

   Gets a parameter of an effect by its index.

   :param effect: Effect object
   :param param:  Parameter index
   :return:       The effect parameter object, or *NULL* if index
                  invalid

---------------------

.. function:: gs_eparam_t *gs_effect_get_param_by_name(const gs_effect_t *effect, const char *name)

   Gets parameter of an effect by its name.

   :param effect: Effect object
   :param name:   Name of the parameter
   :return:       The effect parameter object, or *NULL* if not found

---------------------

.. function:: size_t gs_param_get_num_annotations(const gs_eparam_t *param)

   Gets the number of annotations associated with the parameter.

   :param param:  Param object
   :return:       Number of annotations the param has

---------------------

.. function:: gs_eparam_t *gs_param_get_annotation_by_idx(const gs_eparam_t *param, size_t annotation)

   Gets an annotation of a param by its index.

   :param param:  Param object
   :param param:  Annotation index
   :return:       The effect parameter object (annotation), or *NULL* if index
                  invalid

---------------------

.. function:: gs_eparam_t *gs_param_get_annotation_by_name(const gs_eparam_t *pardam, const char *annotation)

   Gets parameter of an effect by its name.

   :param param:  Param object
   :param name:   Name of the annotation
   :return:       The effect parameter object (annotation), or *NULL* if not found

---------------------

.. function:: bool gs_effect_loop(gs_effect_t *effect, const char *name)

   Helper function that automatically begins techniques/passes.

   :param effect: Effect object
   :param name:   Name of the technique to execute
   :return:       *true* to draw, *false* when complete

   Here is an example of how this function is typically used:

.. code:: cpp

   for (gs_effect_loop(effect, "my_technique")) {
           /* perform drawing here */
           [...]
   }

---------------------

.. function:: gs_eparam_t *gs_effect_get_viewproj_matrix(const gs_effect_t *effect)

   Gets the view/projection matrix parameter ("viewproj") of the effect.

   :param effect: Effect object
   :return:       The view/projection matrix parameter of the effect

---------------------

.. function:: gs_eparam_t *gs_effect_get_world_matrix(const gs_effect_t *effect)

   Gets the world matrix parameter ("world") of the effect.

   :param effect: Effect object
   :return:       The world matrix parameter of the effect

---------------------

.. function:: void gs_effect_get_param_info(const gs_eparam_t *param, struct gs_effect_param_info *info)

   Gets information about an effect parameter.

   :param param: Effect parameter
   :param info:  Pointer to receive the data

   Relevant data types used with this function:

.. code:: cpp

   enum gs_shader_param_type {
           GS_SHADER_PARAM_UNKNOWN,
           GS_SHADER_PARAM_BOOL,
           GS_SHADER_PARAM_FLOAT,
           GS_SHADER_PARAM_INT,
           GS_SHADER_PARAM_STRING,
           GS_SHADER_PARAM_VEC2,
           GS_SHADER_PARAM_VEC3,
           GS_SHADER_PARAM_VEC4,
           GS_SHADER_PARAM_INT2,
           GS_SHADER_PARAM_INT3,
           GS_SHADER_PARAM_INT4,
           GS_SHADER_PARAM_MATRIX4X4,
           GS_SHADER_PARAM_TEXTURE,
   };

   struct gs_effect_param_info {
           const char *name;
           enum gs_shader_param_type type;
   }

---------------------

.. function:: void gs_effect_set_bool(gs_eparam_t *param, bool val)

   Sets a boolean parameter.

   :param param: Effect parameter
   :param val:   Boolean value

---------------------

.. function:: void gs_effect_set_float(gs_eparam_t *param, float val)

   Sets a floating point parameter.

   :param param: Effect parameter
   :param val:   Floating point value

---------------------

.. function:: void gs_effect_set_int(gs_eparam_t *param, int val)

   Sets a integer parameter.

   :param param: Effect parameter
   :param val:   Integer value

---------------------

.. function:: void gs_effect_set_matrix4(gs_eparam_t *param, const struct matrix4 *val)

   Sets a matrix parameter.

   :param param: Effect parameter
   :param val:   Matrix

---------------------

.. function:: void gs_effect_set_vec2(gs_eparam_t *param, const struct vec2 *val)

   Sets a 2-component vector parameter.

   :param param: Effect parameter
   :param val:   Vector

---------------------

.. function:: void gs_effect_set_vec3(gs_eparam_t *param, const struct vec3 *val)

   Sets a 3-component vector parameter.

   :param param: Effect parameter
   :param val:   Vector

---------------------

.. function:: void gs_effect_set_vec4(gs_eparam_t *param, const struct vec4 *val)

   Sets a 4-component vector parameter.

   :param param: Effect parameter
   :param val:   Vector

---------------------

.. function:: void gs_effect_set_color(gs_eparam_t *param, uint32_t argb)

   Convenience function for setting a color value via an integer value.

   :param param: Effect parameter
   :param argb:  Integer color value (i.e. hex value would be
                 0xAARRGGBB)

---------------------

.. function:: void gs_effect_set_texture(gs_eparam_t *param, gs_texture_t *val)

   Sets a texture parameter.

   :param param: Effect parameter
   :param val:   Texture

---------------------

.. function:: void gs_effect_set_texture_srgb(gs_eparam_t *param, gs_texture_t *val)

   Sets a texture parameter using SRGB view if available.

   :param param: Effect parameter
   :param val:   Texture

---------------------

.. function:: void gs_effect_set_val(gs_eparam_t *param, const void *val, size_t size)

   Sets a parameter with data manually.

   :param param: Effect parameter
   :param val:   Pointer to data
   :param size:  Size of data

---------------------

.. function:: void gs_effect_set_default(gs_eparam_t *param)

   Sets the parameter to its default value

   :param: Effect parameter

---------------------

.. function:: void gs_effect_set_next_sampler(gs_eparam_t *param, gs_samplerstate_t *sampler)

   Manually changes the sampler for an effect parameter the next time
   it's used.

   :param param:   Effect parameter
   :param sampler: Sampler state object

---------------------

.. function:: void *gs_effect_get_val(gs_eparam_t *param)

   Returns a copy of the param's current value.

   :param param:   Effect parameter
   :return:        A pointer to the copied byte value of the param's current value. Freed with :c:func:`bfree()`.

---------------------

.. function:: void gs_effect_get_default_val(gs_eparam_t *param)

   Returns a copy of the param's default value.

   :param param:   Effect parameter
   :return:        A pointer to the copied byte value of the param's default value. Freed with :c:func:`bfree()`.

---------------------

.. function:: size_t gs_effect_get_val_size(gs_eparam_t *param)

   Returns the size in bytes of the param's current value.

   :param param:   Effect parameter
   :return:        The size in bytes of the param's current value.

---------------------

.. function:: size_t gs_effect_get_default_val_size(gs_eparam_t *param)

   Returns the size in bytes of the param's default value.

   :param param:   Effect parameter
   :return:        The size in bytes of the param's default value.
