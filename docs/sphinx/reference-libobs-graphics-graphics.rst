Core Graphics API
=================

.. code:: cpp

   #include <graphics/graphics.h>


Graphics Enumerations
---------------------

.. type:: enum gs_draw_mode

   Draw mode.  Can be one of the following values:

   - GS_POINTS    - Draws points
   - GS_LINES     - Draws individual lines
   - GS_LINESTRIP - Draws a line strip
   - GS_TRIS      - Draws individual triangles
   - GS_TRISTRIP  - Draws a triangle strip

.. type:: enum gs_color_format

   Color format.  Can be one of the following values:

   - GS_UNKNOWN     - Unknown format
   - GS_A8          - 8 bit alpha channel only
   - GS_R8          - 8 bit red channel only
   - GS_RGBA        - RGBA, 8 bits per channel
   - GS_BGRX        - BGRX, 8 bits per channel
   - GS_BGRA        - BGRA, 8 bits per channel
   - GS_R10G10B10A2 - RGBA, 10 bits per channel except alpha, which is 2
     bits
   - GS_RGBA16      - RGBA, 16 bits per channel
   - GS_R16         - 16 bit red channel only
   - GS_RGBA16F     - RGBA, 16 bit floating point per channel
   - GS_RGBA32F     - RGBA, 32 bit floating point per channel
   - GS_RG16F       - 16 bit floating point red and green channels only
   - GS_RG32F       - 32 bit floating point red and green channels only
   - GS_R16F        - 16 bit floating point red channel only
   - GS_R32F        - 32 bit floating point red channel only
   - GS_DXT1        - Compressed DXT1
   - GS_DXT3        - Compressed DXT3
   - GS_DXT5        - Compressed DXT5
   - GS_RGBA_UNORM  - RGBA, 8 bits per channel, no SRGB aliasing
   - GS_BGRX_UNORM  - BGRX, 8 bits per channel, no SRGB aliasing
   - GS_BGRA_UNORM  - BGRA, 8 bits per channel, no SRGB aliasing

.. type:: enum gs_zstencil_format

   Z-Stencil buffer format.  Can be one of the following values:

   - GS_ZS_NONE    - No Z-stencil buffer
   - GS_Z16        - 16 bit Z buffer
   - GS_Z24_S8     - 24 bit Z buffer, 8 bit stencil
   - GS_Z32F       - 32 bit floating point Z buffer
   - GS_Z32F_S8X24 - 32 bit floating point Z buffer, 8 bit stencil

.. type:: enum gs_index_type

   Index buffer type.  Can be one of the following values:

   - GS_UNSIGNED_SHORT - 16 bit index
   - GS_UNSIGNED_LONG  - 32 bit index

.. type:: enum gs_cull_mode

   Cull mode.  Can be one of the following values:

   - GS_BACK    - Cull back faces
   - GS_FRONT   - Cull front faces
   - GS_NEITHER - Cull neither

.. type:: enum gs_blend_type

   Blend type.  Can be one of the following values:

   - GS_BLEND_ZERO
   - GS_BLEND_ONE
   - GS_BLEND_SRCCOLOR
   - GS_BLEND_INVSRCCOLOR
   - GS_BLEND_SRCALPHA
   - GS_BLEND_INVSRCALPHA
   - GS_BLEND_DSTCOLOR
   - GS_BLEND_INVDSTCOLOR
   - GS_BLEND_DSTALPHA
   - GS_BLEND_INVDSTALPHA
   - GS_BLEND_SRCALPHASAT

.. type:: enum gs_depth_test

   Depth test type.  Can be one of the following values:

   - GS_NEVER
   - GS_LESS
   - GS_LEQUAL
   - GS_EQUAL
   - GS_GEQUAL
   - GS_GREATER
   - GS_NOTEQUAL
   - GS_ALWAYS

.. type:: enum gs_stencil_side

   Stencil side.  Can be one of the following values:

   - GS_STENCIL_FRONT=1
   - GS_STENCIL_BACK
   - GS_STENCIL_BOTH

.. type:: enum gs_stencil_op_type

   Stencil operation type.  Can be one of the following values:

   - GS_KEEP
   - GS_ZERO
   - GS_REPLACE
   - GS_INCR
   - GS_DECR
   - GS_INVERT

.. type:: enum gs_cube_sides

   Cubemap side.  Can be one of the following values:

   - GS_POSITIVE_X
   - GS_NEGATIVE_X
   - GS_POSITIVE_Y
   - GS_NEGATIVE_Y
   - GS_POSITIVE_Z
   - GS_NEGATIVE_Z

.. type:: enum gs_sample_filter

   Sample filter type.  Can be one of the following values:

   - GS_FILTER_POINT
   - GS_FILTER_LINEAR
   - GS_FILTER_ANISOTROPIC
   - GS_FILTER_MIN_MAG_POINT_MIP_LINEAR
   - GS_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT
   - GS_FILTER_MIN_POINT_MAG_MIP_LINEAR
   - GS_FILTER_MIN_LINEAR_MAG_MIP_POINT
   - GS_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR
   - GS_FILTER_MIN_MAG_LINEAR_MIP_POINT

.. type:: enum gs_address_mode

   Address mode.  Can be one of the following values:

   - GS_ADDRESS_CLAMP
   - GS_ADDRESS_WRAP
   - GS_ADDRESS_MIRROR
   - GS_ADDRESS_BORDER
   - GS_ADDRESS_MIRRORONCE

.. type:: enum gs_texture_type

   Texture type.  Can be one of the following values:

   - GS_TEXTURE_2D
   - GS_TEXTURE_3D
   - GS_TEXTURE_CUBE


Graphics Structures
-------------------

.. type:: struct gs_monitor_info
.. member:: int gs_monitor_info.rotation_degrees
.. member:: long gs_monitor_info.x
.. member:: long gs_monitor_info.y
.. member:: long gs_monitor_info.cx
.. member:: long gs_monitor_info.cy

---------------------

.. type:: struct gs_tvertarray
.. member:: size_t gs_tvertarray.width
.. member:: void *gs_tvertarray.array

---------------------

.. type:: struct gs_vb_data
.. member:: size_t gs_vb_data.num
.. member:: struct vec3 *gs_vb_data.points
.. member:: struct vec3 *gs_vb_data.normals
.. member:: struct vec3 *gs_vb_data.tangents
.. member:: uint32_t *gs_vb_data.colors
.. member:: size_t gs_vb_data.num_tex
.. member:: struct gs_tvertarray *gs_vb_data.tvarray

---------------------

.. type:: struct gs_sampler_info
.. member:: enum gs_sample_filter gs_sampler_info.filter
.. member:: enum gs_address_mode gs_sampler_info.address_u
.. member:: enum gs_address_mode gs_sampler_info.address_v
.. member:: enum gs_address_mode gs_sampler_info.address_w
.. member:: int gs_sampler_info.max_anisotropy
.. member:: uint32_t gs_sampler_info.border_color

---------------------

.. type:: struct gs_display_mode
.. member:: uint32_t gs_display_mode.width
.. member:: uint32_t gs_display_mode.height
.. member:: uint32_t gs_display_mode.bits
.. member:: uint32_t gs_display_mode.freq

---------------------

.. type:: struct gs_rect
.. member:: int gs_rect.x
.. member:: int gs_rect.y
.. member:: int gs_rect.cx
.. member:: int gs_rect.cy

---------------------

.. type:: struct gs_window

   A window structure.  This structure is used with a native widget.

.. member:: void                    *gs_window.hwnd

   (Windows only) an HWND widget.

.. member:: id  gs_window.view

   (Mac only) A view ID.

.. member:: uint32_t gs_window.id
            void* gs_window.display

   (Linux only) Window ID and display

---------------------

.. type:: struct gs_init_data

   Swap chain initialization data.

.. member:: struct gs_window        gs_init_data.window
.. member:: uint32_t                gs_init_data.cx
.. member:: uint32_t                gs_init_data.cy
.. member:: uint32_t                gs_init_data.num_backbuffers
.. member:: enum gs_color_format    gs_init_data.format
.. member:: enum gs_zstencil_format gs_init_data.zsformat
.. member:: uint32_t                gs_init_data.adapter

---------------------


Initialization Functions
------------------------

.. function:: void gs_enum_adapters(bool (*callback)(void *param, const char *name, uint32_t id), void *param)

   Enumerates adapters (this really only applies on windows).

   :param callback: Enumeration callback
   :param param:    Private data passed to the callback

---------------------

.. function:: int gs_create(graphics_t **graphics, const char *module, uint32_t adapter)

   Creates a graphics context

   :param graphics: Pointer to receive the graphics context
   :param module:   Module name
   :param adapter:  Adapter index
   :return:         Can return one of the following values:

                    - GS_SUCCESS
                    - GS_ERROR_FAIL
                    - GS_ERROR_MODULE_NOT_FOUND
                    - GS_ERROR_NOT_SUPPORTED

---------------------

.. function:: void gs_destroy(graphics_t *graphics)

   Destroys a graphics context

   :param graphics: Graphics context

---------------------

.. function:: void gs_enter_context(graphics_t *graphics)

   Enters and locks the graphics context

   :param graphics: Graphics context

---------------------

.. function:: void gs_leave_context(void)

   Leaves and unlocks the graphics context

   :param graphics: Graphics context

---------------------

.. function:: graphics_t *gs_get_context(void)

   :return: The currently locked graphics context for this thread

---------------------


Matrix Stack Functions
----------------------

.. function:: void gs_matrix_push(void)

   Pushes the matrix stack and duplicates the current matrix.

---------------------

.. function:: void gs_matrix_pop(void)

   Pops the current matrix from the matrix stack.

---------------------

.. function:: void gs_matrix_identity(void)

   Sets the current matrix to an identity matrix.

---------------------

.. function:: void gs_matrix_transpose(void)

   Transposes the current matrix.

---------------------

.. function:: void gs_matrix_set(const struct matrix4 *matrix)

   Sets the current matrix.

   :param matrix: The matrix to set

---------------------

.. function:: void gs_matrix_get(struct matrix4 *dst)

   Gets the current matrix

   :param dst: Destination matrix

---------------------

.. function:: void gs_matrix_mul(const struct matrix4 *matrix)

   Multiplies the current matrix

   :param matrix: Matrix to multiply the current stack matrix with

---------------------

.. function:: void gs_matrix_rotquat(const struct quat *rot)

   Multiplies the current matrix with a quaternion

   :param rot: Quaternion to multiple the current matrix stack with

---------------------

.. function:: void gs_matrix_rotaa(const struct axisang *rot)
              void gs_matrix_rotaa4f(float x, float y, float z, float angle)

   Multiplies the current matrix with an axis angle

   :param rot: Axis angle to multiple the current matrix stack with

---------------------

.. function:: void gs_matrix_translate(const struct vec3 *pos)
              void gs_matrix_translate3f(float x, float y, float z)

   Translates the current matrix

   :param pos: Vector to translate the current matrix stack with

---------------------

.. function:: void gs_matrix_scale(const struct vec3 *scale)
              void gs_matrix_scale3f(float x, float y, float z)

   Scales the current matrix

   :param scale: Scale value to scale the current matrix stack with

---------------------


Draw Functions
--------------

.. function:: gs_effect_t *gs_get_effect(void)

   :return: The currently active effect, or *NULL* if none active

---------------------

.. function:: void gs_draw_sprite(gs_texture_t *tex, uint32_t flip, uint32_t width, uint32_t height)

   Draws a 2D sprite.  Sets the "image" parameter of the current effect
   to the texture and renders a quad.

   If width or height is 0, the width or height of the texture will be
   used.  The flip value specifies whether the texture should be flipped
   on the U or V axis with GS_FLIP_U and GS_FLIP_V.

   :param tex:    Texture to draw
   :param flip:   Can be 0 or a bitwise-OR combination of one of the
                  following values:

                  - GS_FLIP_U - Flips the texture horizontally
                  - GS_FLIP_V - Flips the texture vertically

   :param width:  Width
   :param height: Height

---------------------

.. function:: void gs_draw_sprite_subregion(gs_texture_t *tex, uint32_t flip, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy)

   Draws a subregion of a 2D sprite.  Sets the "image" parameter of the
   current effect to the texture and renders a quad.

   :param tex:    Texture to draw
   :param flip:   Can be 0 or a bitwise-OR combination of one of the
                  following values:

                  - GS_FLIP_U - Flips the texture horizontally
                  - GS_FLIP_V - Flips the texture vertically

   :param x:      X value within subregion
   :param y:      Y value within subregion
   :param cx:     CX value of subregion
   :param cy:     CY value of subregion

---------------------

.. function:: void gs_reset_viewport(void)

    Sets the viewport to current swap chain size

---------------------

.. function:: void gs_set_2d_mode(void)

    Sets the projection matrix to a default screen-sized orthographic
    mode

---------------------

.. function:: void gs_set_3d_mode(double fovy, double znear, double zfar)

    Sets the projection matrix to a default screen-sized perspective
    mode

    :param fovy:  Field of view (in degrees)
    :param znear: Near plane
    :param zfar:  Far plane

---------------------

.. function:: void gs_viewport_push(void)

   Pushes/stores the current viewport

---------------------

.. function:: void gs_viewport_pop(void)

   Pops/recalls the last pushed viewport

---------------------

.. function:: void gs_perspective(float fovy, float aspect, float znear, float zfar)

   Sets the projection matrix to a perspective mode

   :param fovy:   Field of view (in degrees)
   :param aspect: Aspect ratio
   :param znear:  Near plane
   :param zfar:   Far plane

---------------------

.. function:: void gs_blend_state_push(void)

   Pushes/stores the current blend state

---------------------

.. function:: void gs_blend_state_pop(void)

   Pops/restores the last blend state

---------------------

.. function:: void gs_reset_blend_state(void)

   Sets the blend state to the default value: source alpha and invert
   source alpha.

---------------------


Swap Chains
-----------

.. function:: gs_swapchain_t *gs_swapchain_create(const struct gs_init_data *data)

   Creates a swap chain (display view on a native widget)

   :param data: Swap chain initialization data
   :return:     New swap chain object, or *NULL* if failed

---------------------

.. function:: void     gs_swapchain_destroy(gs_swapchain_t *swapchain)

   Destroys a swap chain

---------------------

.. function:: void gs_resize(uint32_t cx, uint32_t cy)

   Resizes the currently active swap chain

   :param cx: New width
   :param cy: New height

---------------------

.. function:: void gs_get_size(uint32_t *cx, uint32_t *cy)

   Gets the size of the currently active swap chain

   :param cx: Pointer to receive width
   :param cy: Pointer to receive height

---------------------

.. function:: uint32_t gs_get_width(void)

   Gets the width of the currently active swap chain

---------------------

.. function:: uint32_t gs_get_height(void)

   Gets the height of the currently active swap chain

---------------------


Resource Loading
----------------

.. function:: void gs_load_vertexbuffer(gs_vertbuffer_t *vertbuffer)

   Loads a vertex buffer

   :param vertbuffer: Vertex buffer to load, or NULL to unload

---------------------

.. function:: void gs_load_indexbuffer(gs_indexbuffer_t *indexbuffer)

   Loads a index buffer

   :param indexbuffer: Index buffer to load, or NULL to unload

---------------------

.. function:: void gs_load_texture(gs_texture_t *tex, int unit)

   Loads a texture (this is usually not called manually)

   :param tex:  Texture to load, or NULL to unload
   :param unit: Texture unit to load texture for

---------------------

.. function:: void gs_load_samplerstate(gs_samplerstate_t *samplerstate, int unit)

   Loads a sampler state (this is usually not called manually)

   :param samplerstate: Sampler state to load, or NULL to unload
   :param unit:         Texture unit to load sampler state for

---------------------

.. function:: void gs_load_swapchain(gs_swapchain_t *swapchain)

   Loads a swapchain

   :param swapchain: Swap chain to load, or NULL to unload

---------------------


Draw Functions
--------------

.. function:: gs_texture_t  *gs_get_render_target(void)

   :return: The currently active render target

---------------------

.. function:: gs_zstencil_t *gs_get_zstencil_target(void)

   :return: The currently active Z-stencil target

---------------------

.. function:: void gs_set_render_target(gs_texture_t *tex, gs_zstencil_t *zstencil)

   Sets the active render target

   :param tex:      Texture to set as the active render target
   :param zstencil: Z-stencil to use as the active render target

---------------------

.. function:: void gs_set_cube_render_target(gs_texture_t *cubetex, int side, gs_zstencil_t *zstencil)

   Sets a cubemap side as the active render target

   :param cubetex:  Cubemap
   :param side:     Cubemap side
   :param zstencil: Z-stencil buffer, or *NULL* if none

---------------------

.. function:: void gs_copy_texture(gs_texture_t *dst, gs_texture_t *src)

   Copies a texture

   :param dst: Destination texture
   :param src: Source texture

---------------------

.. function:: void gs_stage_texture(gs_stagesurf_t *dst, gs_texture_t *src)

   Copies a texture to a staging surface and copies it to RAM.  Ideally
   best to give this a frame to process to prevent stalling.

   :param dst: Staging surface
   :param src: Texture to stage

---------------------

.. function:: void gs_begin_scene(void)
              void gs_end_scene(void)

   Begins/ends a scene (this is automatically called by libobs, there's
   no need to call this manually).

---------------------

.. function:: void gs_draw(enum gs_draw_mode draw_mode, uint32_t start_vert, uint32_t num_verts)

   Draws a primitive or set of primitives.

   :param draw_mode:  The primitive draw mode to use
   :param start_vert: Starting vertex index
   :param num_verts:  Number of vertices

---------------------

.. function:: void gs_clear(uint32_t clear_flags, const struct vec4 *color, float depth, uint8_t stencil)

   Clears color/depth/stencil buffers.

   :param clear_flags: Flags to clear with.  Can be one of the following
                       values:

                       - GS_CLEAR_COLOR   - Clears color buffer
                       - GS_CLEAR_DEPTH   - Clears depth buffer
                       - GS_CLEAR_STENCIL - Clears stencil buffer

   :param color:       Color value to clear the color buffer with
   :param depth:       Depth value to clear the depth buffer with
   :param stencil:     Stencil value to clear the stencil buffer with

---------------------

.. function:: void gs_present(void)

   Displays what was rendered on to the current render target

---------------------

.. function:: void gs_flush(void)

   Flushes GPU calls

---------------------

.. function:: void gs_set_cull_mode(enum gs_cull_mode mode)

   Sets the current cull mode.

   :param mode: Cull mode

---------------------

.. function:: enum gs_cull_mode gs_get_cull_mode(void)

   :return: The current cull mode

---------------------

.. function:: void gs_enable_blending(bool enable)

   Enables/disables blending

   :param enable: *true* to enable, *false* to disable

---------------------

.. function:: void gs_enable_depth_test(bool enable)

   Enables/disables depth testing

   :param enable: *true* to enable, *false* to disable

---------------------

.. function:: void gs_enable_stencil_test(bool enable)

   Enables/disables stencil testing

   :param enable: *true* to enable, *false* to disable

---------------------

.. function:: void gs_enable_stencil_write(bool enable)

   Enables/disables stencil writing

   :param enable: *true* to enable, *false* to disable

---------------------

.. function:: void gs_enable_color(bool red, bool green, bool blue, bool alpha)

   Enables/disables specific color channels

   :param red:   *true* to enable red channel, *false* to disable
   :param green: *true* to enable green channel, *false* to disable
   :param blue:  *true* to enable blue channel, *false* to disable
   :param alpha: *true* to enable alpha channel, *false* to disable

---------------------

.. function:: void gs_blend_function(enum gs_blend_type src, enum gs_blend_type dest)

   Sets the blend function

   :param src:  Blend type for the source
   :param dest: Blend type for the destination

---------------------

.. function:: void gs_blend_function_separate(enum gs_blend_type src_c, enum gs_blend_type dest_c, enum gs_blend_type src_a, enum gs_blend_type dest_a)

   Sets the blend function for RGB and alpha separately

   :param src_c:  Blend type for the source RGB
   :param dest_c: Blend type for the destination RGB
   :param src_a:  Blend type for the source alpha
   :param dest_a: Blend type for the destination alpha

---------------------

.. function:: void gs_depth_function(enum gs_depth_test test)

   Sets the depth function

   :param test: Sets the depth test type

---------------------

.. function:: void gs_stencil_function(enum gs_stencil_side side, enum gs_depth_test test)

   Sets the stencil function

   :param side: Stencil side
   :param test: Depth test

---------------------

.. function:: void gs_stencil_op(enum gs_stencil_side side, enum gs_stencil_op_type fail, enum gs_stencil_op_type zfail, enum gs_stencil_op_type zpass)

   Sets the stencil operation

   :param side:  Stencil side
   :param fail:  Operation to perform on stencil test failure
   :param zfail: Operation to perform on depth test failure
   :param zpass: Operation to perform on depth test success

---------------------

.. function:: void gs_set_viewport(int x, int y, int width, int height)

   Sets the current viewport

   :param x:      X position relative to upper left
   :param y:      Y position relative to upper left
   :param width:  Width of the viewport
   :param height: Height of the viewport

---------------------

.. function:: void gs_get_viewport(struct gs_rect *rect)

   Gets the current viewport

   :param rect: Pointer to receive viewport rectangle

---------------------

.. function:: void gs_set_scissor_rect(const struct gs_rect *rect)

   Sets or clears the current scissor rectangle

   :rect: Scissor rectangle, or *NULL* to clear

---------------------

.. function:: void gs_ortho(float left, float right, float top, float bottom, float znear, float zfar)

   Sets the projection matrix to an orthographic matrix

---------------------

.. function:: void gs_frustum(float left, float right, float top, float bottom, float znear, float zfar)

   Sets the projection matrix to a frustum matrix

---------------------

.. function:: void gs_projection_push(void)

   Pushes/stores the current projection matrix

---------------------

.. function:: void gs_projection_pop(void)

   Pops/restores the last projection matrix pushed

---------------------


Texture Functions
-----------------

.. function:: gs_texture_t *gs_texture_create(uint32_t width, uint32_t height, enum gs_color_format color_format, uint32_t levels, const uint8_t **data, uint32_t flags)

   Creates a texture.

   :param width:        Width
   :param height:       Height
   :param color_format: Color format
   :param levels:       Number of total texture levels.  Set to 1 if no
                        mip-mapping
   :param data:         Pointer to array of texture data pointers
   :param flags:        Can be 0 or a bitwise-OR combination of one or
                        more of the following value:
                        
                        - GS_BUILD_MIPMAPS - Automatically builds
                          mipmaps (Note: not fully tested)
                        - GS_DYNAMIC - Dynamic
                        - GS_RENDER_TARGET - Render target

   :return:             A new texture object

---------------------

.. function:: gs_texture_t *gs_texture_create_from_file(const char *file)

   Creates a texture from a file.  Note that this isn't recommended for
   animated gifs -- instead use the :ref:`image_file_helper`.

   :param file: Image file to open

---------------------

.. function:: void     gs_texture_destroy(gs_texture_t *tex)

   Destroys a texture

   :param tex: Texture object

---------------------

.. function:: uint32_t gs_texture_get_width(const gs_texture_t *tex)

   Gets the texture's width

   :param tex: Texture object
   :return:    The texture's width

---------------------

.. function:: uint32_t gs_texture_get_height(const gs_texture_t *tex)

   Gets the texture's height

   :param tex: Texture object
   :return:    The texture's height

---------------------

.. function:: enum gs_color_format gs_texture_get_color_format(const gs_texture_t *tex)

   Gets the texture's color format

   :param tex: Texture object
   :return:    The texture's color format

---------------------

.. function:: bool     gs_texture_map(gs_texture_t *tex, uint8_t **ptr, uint32_t *linesize)

   Maps a texture.

   :param tex:      Texture object
   :param ptr:      Pointer to receive the pointer to the texture data
                    to write to
   :param linesize: Pointer to receive the line size (pitch) of the
                    texture

---------------------

.. function:: void     gs_texture_unmap(gs_texture_t *tex)

   Unmaps a texture.

   :param tex: Texture object

---------------------

.. function:: void gs_texture_set_image(gs_texture_t *tex, const uint8_t *data, uint32_t linesize, bool invert)

   Sets the image of a dynamic texture

   :param tex:      Texture object
   :param data:     Data to set as the image
   :param linesize: Line size (pitch) of the data
   :param invert:   *true* to invert vertically, *false* otherwise

---------------------

.. function:: gs_texture_t *gs_texture_create_from_iosurface(void *iosurf)

   **Mac only:** Creates a texture from an IOSurface.

   :param iosurf: IOSurface object

---------------------

.. function:: bool     gs_texture_rebind_iosurface(gs_texture_t *texture, void *iosurf)

   **Mac only:** Rebinds a texture to another IOSurface

   :param texture: Texture object
   :param iosuf:   IOSurface object

---------------------

.. function:: gs_texture_t *gs_texture_create_gdi(uint32_t width, uint32_t height)

   **Windows only:** Creates a GDI-interop texture

   :param width:  Width
   :param height: Height

---------------------

.. function:: void *gs_texture_get_dc(gs_texture_t *gdi_tex)

   **Windows only:** Gets the HDC of a GDI-interop texture.  Call
   :c:func:`gs_texture_release_dc()` to release the HDC.

   :param gdi_tex: GDI-interop texture object
   :return:        HDC object

---------------------

.. function:: void gs_texture_release_dc(gs_texture_t *gdi_tex)

   **Windows only:** Releases the HDC of the GDI-interop texture.

   :param gdi_tex: GDI-interop texture object

---------------------

.. function:: gs_texture_t *gs_texture_open_shared(uint32_t handle)

   **Windows only:** Creates a texture from a shared texture handle.

   :param handle: Shared texture handle
   :return:       A texture object

---------------------

.. function:: bool gs_gdi_texture_available(void)

   **Windows only:** Returns whether GDI-interop textures are available.

   :return: *true* if available, *false* otherwise

---------------------

.. function:: bool gs_shared_texture_available(void)

   **Windows only:** Returns whether shared textures are available.

   :return: *true* if available, *false* otherwise

---------------------


Cube Texture Functions
----------------------

.. function:: gs_texture_t *gs_cubetexture_create(uint32_t size, enum gs_color_format color_format, uint32_t levels, const uint8_t **data, uint32_t flags)

   Creates a cubemap texture.

   :param size:         Width/height/depth value
   :param color_format: Color format
   :param levels:       Number of texture levels
   :param data:         Pointer to array of texture data pointers
   :param flags:        Can be 0 or a bitwise-OR combination of one or
                        more of the following value:
                        
                        - GS_BUILD_MIPMAPS - Automatically builds
                          mipmaps (Note: not fully tested)
                        - GS_DYNAMIC - Dynamic
                        - GS_RENDER_TARGET - Render target

   :return:             A new cube texture object

---------------------

.. function:: void     gs_cubetexture_destroy(gs_texture_t *cubetex)

   Destroys a cube texture.

   :param cubetex: Cube texture object

---------------------

.. function:: uint32_t gs_cubetexture_get_size(const gs_texture_t *cubetex)

   Get the width/height/depth value of a cube texture.

   :param cubetex: Cube texture object
   :return:        The width/height/depth value of the cube texture

---------------------

.. function:: enum gs_color_format gs_cubetexture_get_color_format(const gs_texture_t *cubetex)

   Gets the color format of a cube texture.

   :param cubetex: Cube texture object
   :return:        The color format of the cube texture

---------------------

.. function:: void gs_cubetexture_set_image(gs_texture_t *cubetex, uint32_t side, const void *data, uint32_t linesize, bool invert)

   Sets an image of a cube texture side.

   :param cubetex:  Cube texture object
   :param side:     Side
   :param data:     Texture data to set
   :param linesize: Line size (pitch) of the texture data
   :param invert:   *true* to invert texture data, *false* otherwise

---------------------


Staging Surface Functions
-------------------------

Staging surfaces are used to efficiently copy textures from VRAM to RAM.

.. function:: gs_stagesurf_t *gs_stagesurface_create(uint32_t width, uint32_t height, enum gs_color_format color_format)

   Creates a staging surface.

   :param width:        Width
   :param height:       Height
   :param color_format: Color format
   :return:             The staging surface object

---------------------

.. function:: void     gs_stagesurface_destroy(gs_stagesurf_t *stagesurf)

   Destroys a staging surface.

   :param stagesurf: Staging surface object

---------------------

.. function:: uint32_t gs_stagesurface_get_width(const gs_stagesurf_t *stagesurf)
              uint32_t gs_stagesurface_get_height(const gs_stagesurf_t *stagesurf)

   Gets the width/height of a staging surface object.

   :param stagesurf: Staging surface object
   :return:          Width/height of the staging surface

---------------------

.. function:: enum gs_color_format gs_stagesurface_get_color_format(const gs_stagesurf_t *stagesurf)

   Gets the color format of a staging surface object.

   :param stagesurf: Staging surface object
   :return:          Color format of the staging surface

---------------------

.. function:: bool     gs_stagesurface_map(gs_stagesurf_t *stagesurf, uint8_t **data, uint32_t *linesize)

   Maps the staging surface texture (for reading).  Call
   :c:func:`gs_stagesurface_unmap()` to unmap when complete.

   :param stagesurf: Staging surface object
   :param data:      Pointer to receive texture data pointer
   :param linesize:  Pointer to receive line size (pitch) of the texture
                     data
   :return:          *true* if map successful, *false* otherwise

---------------------

.. function:: void     gs_stagesurface_unmap(gs_stagesurf_t *stagesurf)

   Unmaps a staging surface.

   :param stagesurf: Staging surface object

---------------------


Z-Stencil Functions
-------------------

.. function:: gs_zstencil_t *gs_zstencil_create(uint32_t width, uint32_t height, enum gs_zstencil_format format)

   Creates a Z-stencil surface object.

   :param width:  Width
   :param height: Height
   :param format: Format
   :return:       New Z-stencil surface object, or *NULL* if failed

---------------------

.. function:: void     gs_zstencil_destroy(gs_zstencil_t *zstencil)

   Destroys a Z-stencil buffer.

   :param zstencil: Z-stencil surface object

---------------------


Sampler State Functions
-----------------------

.. function:: gs_samplerstate_t *gs_samplerstate_create(const struct gs_sampler_info *info)

   Creates a sampler state object.

   :param info: Sampler state information
   :return:     New sampler state object

---------------------

.. function:: void     gs_samplerstate_destroy(gs_samplerstate_t *samplerstate)

   Destroys a sampler state object.

   :param samplerstate: Sampler state object

---------------------


Vertex Buffer Functions
-----------------------

.. function:: gs_vertbuffer_t *gs_vertexbuffer_create(struct gs_vb_data *data, uint32_t flags)

   Creates a vertex buffer.

   :param data:  Vertex buffer data to create vertex buffer with.  The
                 structure should be created with gs_vbdata_create(),
                 and then buffers in this structure should be allocated
                 with :c:func:`bmalloc()`, :c:func:`bzalloc()`, or
                 :c:func:`brealloc()`.  The ownership of the gs_vb_data
                 pointer is then passed to the function, and they should
                 not be destroyed by the caller once passed

   :param flags: Creation flags.  Can be 0 or a bitwise-OR combination
                 of any of the following values:

                 - GS_DYNAMIC - Can be dynamically updated in real time.
                 - GS_DUP_BUFFER - Do not pass buffer ownership of the
                   structure or the buffer pointers within the
                   structure.

   :return:      A new vertex buffer object, or *NULL* if failed

---------------------

.. function:: void     gs_vertexbuffer_destroy(gs_vertbuffer_t *vertbuffer)

   Destroys a vertex buffer object.

   :param vertbuffer: Vertex buffer object

---------------------

.. function:: void     gs_vertexbuffer_flush(gs_vertbuffer_t *vertbuffer)

   Flushes a vertex buffer to its interval vertex data object.  To
   modify its internal vertex data, call
   :c:func:`gs_vertexbuffer_get_data()`.

   Can only be used with dynamic vertex buffer objects.

   :param vertbuffer: Vertex buffer object

---------------------

.. function:: void     gs_vertexbuffer_flush_direct(gs_vertbuffer_t *vertbuffer, const struct gs_vb_data *data)

   Directly flushes a vertex buffer to the specified vertex buffer data.
   .

   Can only be used with dynamic vertex buffer objects.

   :param vertbuffer: Vertex buffer object
   :param data:       Vertex buffer data to flush.  Components that
                      don't need to be flushed can be left *NULL*

---------------------

.. function:: struct gs_vb_data *gs_vertexbuffer_get_data(const gs_vertbuffer_t *vertbuffer)

   Gets the vertex buffer data associated with a vertex buffer object.
   This data can be changed and vertex buffer can be updated with
   :c:func:`gs_vertexbuffer_flush()`.

   Can only be used with dynamic vertex buffer objects.

   :param vertbuffer: Vertex buffer object
   :return:           Vertex buffer data structure

---------------------


Index Buffer Functions
----------------------

.. function:: gs_indexbuffer_t *gs_indexbuffer_create(enum gs_index_type type, void *indices, size_t num, uint32_t flags)

   Creates an index buffer.

   :param type:    Index buffer type
   :param indices: Index buffer data.  This buffer must be allocated
                   with :c:func:`bmalloc()`, :c:func:`bzalloc()`, or
                   :c:func:`bralloc()`, and ownership of this buffer is
                   passed to the index buffer object.
   :param num:     Number of indices in the buffer

   :param flags:   Creation flags.  Can be 0 or a bitwise-OR combination
                   of any of the following values:

                   - GS_DYNAMIC - Can be dynamically updated in real time.
                   - GS_DUP_BUFFER - Do not pass buffer ownership

   :return:        A new index buffer object, or *NULL* if failed

---------------------

.. function:: void     gs_indexbuffer_destroy(gs_indexbuffer_t *indexbuffer)

   Destroys an index buffer object.

   :param indexbuffer: Index buffer object

---------------------

.. function:: void     gs_indexbuffer_flush(gs_indexbuffer_t *indexbuffer)

   Flushes a index buffer to its interval index data object.  To modify
   its internal index data, call :c:func:`gs_indexbuffer_get_data()`.

   Can only be used with dynamic index buffer objects.

   :param indexbuffer: Index buffer object

---------------------

.. function:: void     gs_indexbuffer_flush_direct(gs_indexbuffer_t *indexbuffer, const void *data)

   Flushes a index buffer to the specified index buffer data.

   Can only be used with dynamic index buffer objects.

   :param indexbuffer: Index buffer object
   :param data:        Index buffer data to flush

---------------------

.. function:: void     *gs_indexbuffer_get_data(const gs_indexbuffer_t *indexbuffer)

   Gets the index buffer data associated with a index buffer object.
   This data can be changed and index buffer can be updated with
   :c:func:`gs_indexbuffer_flush()`.

   Can only be used with dynamic index buffer objects.

   :param vertbuffer: Index buffer object
   :return:           Index buffer data pointer

---------------------

.. function:: size_t   gs_indexbuffer_get_num_indices(const gs_indexbuffer_t *indexbuffer)

   Gets the number of indices associated with this index buffer.

   :param indexbuffer: Index buffer object
   :return:            Number of indices the vertex buffer object has

---------------------

.. function:: enum gs_index_type gs_indexbuffer_get_type(const gs_indexbuffer_t *indexbuffer)

   Gets the type of index buffer.

   :param indexbuffer: Index buffer object
   :return:            Index buffer type

---------------------


Display Duplicator (Windows Only)
---------------------------------

.. function:: gs_duplicator_t *gs_duplicator_create(int monitor_idx)

---------------------

.. function:: void gs_duplicator_destroy(gs_duplicator_t *duplicator)

---------------------

.. function:: bool gs_duplicator_update_frame(gs_duplicator_t *duplicator)

---------------------

.. function:: gs_texture_t *gs_duplicator_get_texture(gs_duplicator_t *duplicator)

---------------------

.. function:: bool gs_get_duplicator_monitor_info(int monitor_idx, struct gs_monitor_info *monitor_info)

---------------------


Render Helper Functions
-----------------------

.. function:: void gs_render_start(bool b_new)

---------------------

.. function:: void gs_render_stop(enum gs_draw_mode mode)

---------------------

.. function:: gs_vertbuffer_t *gs_render_save(void)

---------------------

.. function:: void gs_vertex2f(float x, float y)

---------------------

.. function:: void gs_vertex3f(float x, float y, float z)

---------------------

.. function:: void gs_normal3f(float x, float y, float z)

---------------------

.. function:: void gs_color(uint32_t color)

---------------------

.. function:: void gs_texcoord(float x, float y, int unit)

---------------------

.. function:: void gs_vertex2v(const struct vec2 *v)

---------------------

.. function:: void gs_vertex3v(const struct vec3 *v)

---------------------

.. function:: void gs_normal3v(const struct vec3 *v)

---------------------

.. function:: void gs_color4v(const struct vec4 *v)

---------------------

.. function:: void gs_texcoord2v(const struct vec2 *v, int unit)

---------------------


Graphics Types
--------------

.. type:: typedef struct gs_duplicator       gs_duplicator_t
.. type:: typedef struct gs_texture          gs_texture_t
.. type:: typedef struct gs_stage_surface    gs_stagesurf_t
.. type:: typedef struct gs_zstencil_buffer  gs_zstencil_t
.. type:: typedef struct gs_vertex_buffer    gs_vertbuffer_t
.. type:: typedef struct gs_index_buffer     gs_indexbuffer_t
.. type:: typedef struct gs_sampler_state    gs_samplerstate_t
.. type:: typedef struct gs_swap_chain       gs_swapchain_t
.. type:: typedef struct gs_texture_render   gs_texrender_t
.. type:: typedef struct gs_shader           gs_shader_t
.. type:: typedef struct gs_shader_param     gs_sparam_t
.. type:: typedef struct gs_device           gs_device_t
.. type:: typedef struct graphics_subsystem  graphics_t
