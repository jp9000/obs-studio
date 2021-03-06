#pragma once

#include "gl-nix.h"

#include <glad/glad_egl.h>

const char *gl_egl_error_to_string(EGLint error_number);

struct gs_texture *
gl_egl_create_dmabuf_image(EGLDisplay egl_display, unsigned int width,
			   unsigned int height,
			   enum gs_color_format color_format, uint32_t n_planes,
			   const int *fds, const uint32_t *strides,
			   const uint32_t *offsets, const uint64_t *modifiers);
