/******************************************************************************
    Copyright (C) 2020 by Georges Basile Stavracas Neto <georges.stavracas@gmail.com>

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

#include "gl-egl-common.h"

#include <stdio.h>
#include <stdlib.h>

#include <glad/glad_egl.h>

#if defined(__linux__)

#include <linux/types.h>
#include <asm/ioctl.h>
typedef unsigned int drm_handle_t;

#else

#include <stdint.h>
#include <sys/ioccom.h>
#include <sys/types.h>
typedef int8_t __s8;
typedef uint8_t __u8;
typedef int16_t __s16;
typedef uint16_t __u16;
typedef int32_t __s32;
typedef uint32_t __u32;
typedef int64_t __s64;
typedef uint64_t __u64;
typedef size_t __kernel_size_t;
typedef unsigned long drm_handle_t;

#endif

typedef void(APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(
	GLenum target, GLeglImageOES image);
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

/* copied from drm_fourcc.h */

#define fourcc_code(a, b, c, d)                                \
	((__u32)(a) | ((__u32)(b) << 8) | ((__u32)(c) << 16) | \
	 ((__u32)(d) << 24))
#define DRM_FORMAT_INVALID 0
#define DRM_FORMAT_R8 fourcc_code('R', '8', ' ', ' ') /* [7:0] R */
#define DRM_FORMAT_R16 \
	fourcc_code('R', '1', '6', ' ') /* [15:0] R little endian */
#define DRM_FORMAT_RG88 \
	fourcc_code('R', 'G', '8', '8') /* [15:0] R:G 8:8 little endian */
#define DRM_FORMAT_ABGR8888        \
	fourcc_code('A', 'B', '2', \
		    '4') /* [31:0] A:B:G:R 8:8:8:8 little endian */
#define DRM_FORMAT_ABGR2101010     \
	fourcc_code('A', 'B', '3', \
		    '0') /* [31:0] A:B:G:R 2:10:10:10 little endian */
#define DRM_FORMAT_ABGR16161616F   \
	fourcc_code('A', 'B', '4', \
		    'H') /* [63:0] A:B:G:R 16:16:16:16 little endian */
#define DRM_FORMAT_ARGB8888        \
	fourcc_code('A', 'R', '2', \
		    '4') /* [31:0] A:R:G:B 8:8:8:8 little endian */
#define DRM_FORMAT_XRGB8888        \
	fourcc_code('X', 'R', '2', \
		    '4') /* [31:0] x:R:G:B 8:8:8:8 little endian */

static bool find_gl_extension(const char *extension)
{
	GLint n, i;

	glGetIntegerv(GL_NUM_EXTENSIONS, &n);
	for (i = 0; i < n; i++) {
		const char *e = (char *)glGetStringi(GL_EXTENSIONS, i);
		if (extension && strcmp(e, extension) == 0)
			return true;
	}
	return false;
}

static bool init_egl_image_target_texture_2d_ext(void)
{
	static bool initialized = false;

	if (!initialized) {
		initialized = true;

		if (!find_gl_extension("GL_OES_EGL_image")) {
			blog(LOG_ERROR, "No GL_OES_EGL_image");
			return false;
		}

		glEGLImageTargetTexture2DOES =
			(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress(
				"glEGLImageTargetTexture2DOES");
	}

	if (!glEGLImageTargetTexture2DOES)
		return false;

	return true;
}

static inline enum gs_color_format gs_format_to_drm_format(uint32_t drm_format)
{
	switch (drm_format) {
	case GS_R8:
		return DRM_FORMAT_R8;
	case GS_RGBA:
		return DRM_FORMAT_ABGR8888;
	case GS_BGRX:
		return DRM_FORMAT_XRGB8888;
	case GS_BGRA:
		return DRM_FORMAT_ARGB8888;
	case GS_R10G10B10A2:
		return DRM_FORMAT_ABGR2101010;
	case GS_R16:
		return DRM_FORMAT_R16;
	case GS_RGBA16F:
		return DRM_FORMAT_ABGR16161616F;
	case GS_R8G8:
		return DRM_FORMAT_RG88;
	case GS_A8:
	case GS_R16F:
	case GS_RGBA16:
	case GS_RG16F:
	case GS_R32F:
	case GS_RG32F:
	case GS_RGBA32F:
	case GS_DXT1:
	case GS_DXT3:
	case GS_DXT5:
	case GS_UNKNOWN:
		return DRM_FORMAT_INVALID;
	}

	return DRM_FORMAT_INVALID;
}

static EGLImageKHR
create_dmabuf_egl_image(EGLDisplay egl_display, unsigned int width,
			unsigned int height, uint32_t drm_format,
			uint32_t n_planes, const int *fds,
			const uint32_t *strides, const uint32_t *offsets,
			const uint64_t *modifiers)
{
	EGLAttrib attribs[47];
	int atti = 0;

	/* This requires the Mesa commit in
	 * Mesa 10.3 (08264e5dad4df448e7718e782ad9077902089a07) or
	 * Mesa 10.2.7 (55d28925e6109a4afd61f109e845a8a51bd17652).
	 * Otherwise Mesa closes the fd behind our back and re-importing
	 * will fail.
	 * https://bugs.freedesktop.org/show_bug.cgi?id=76188
	 * */

	attribs[atti++] = EGL_WIDTH;
	attribs[atti++] = width;
	attribs[atti++] = EGL_HEIGHT;
	attribs[atti++] = height;
	attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
	attribs[atti++] = drm_format;

	if (n_planes > 0) {
		attribs[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
		attribs[atti++] = fds[0];
		attribs[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
		attribs[atti++] = offsets[0];
		attribs[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
		attribs[atti++] = strides[0];
		if (modifiers) {
			attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
			attribs[atti++] = modifiers[0] & 0xFFFFFFFF;
			attribs[atti++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
			attribs[atti++] = modifiers[0] >> 32;
		}
	}

	if (n_planes > 1) {
		attribs[atti++] = EGL_DMA_BUF_PLANE1_FD_EXT;
		attribs[atti++] = fds[1];
		attribs[atti++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
		attribs[atti++] = offsets[1];
		attribs[atti++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
		attribs[atti++] = strides[1];
		if (modifiers) {
			attribs[atti++] = EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT;
			attribs[atti++] = modifiers[1] & 0xFFFFFFFF;
			attribs[atti++] = EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT;
			attribs[atti++] = modifiers[1] >> 32;
		}
	}

	if (n_planes > 2) {
		attribs[atti++] = EGL_DMA_BUF_PLANE2_FD_EXT;
		attribs[atti++] = fds[2];
		attribs[atti++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
		attribs[atti++] = offsets[2];
		attribs[atti++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
		attribs[atti++] = strides[2];
		if (modifiers) {
			attribs[atti++] = EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT;
			attribs[atti++] = modifiers[2] & 0xFFFFFFFF;
			attribs[atti++] = EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT;
			attribs[atti++] = modifiers[2] >> 32;
		}
	}

	if (n_planes > 3) {
		attribs[atti++] = EGL_DMA_BUF_PLANE3_FD_EXT;
		attribs[atti++] = fds[3];
		attribs[atti++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
		attribs[atti++] = offsets[3];
		attribs[atti++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
		attribs[atti++] = strides[3];
		if (modifiers) {
			attribs[atti++] = EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT;
			attribs[atti++] = modifiers[3] & 0xFFFFFFFF;
			attribs[atti++] = EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT;
			attribs[atti++] = modifiers[3] >> 32;
		}
	}

	attribs[atti++] = EGL_NONE;

	return eglCreateImage(egl_display, EGL_NO_CONTEXT,
			      EGL_LINUX_DMA_BUF_EXT, 0, attribs);
}

struct gs_texture *
gl_egl_create_dmabuf_image(EGLDisplay egl_display, unsigned int width,
			   unsigned int height,
			   enum gs_color_format color_format, uint32_t n_planes,
			   const int *fds, const uint32_t *strides,
			   const uint32_t *offsets, const uint64_t *modifiers)
{
	struct gs_texture *texture = NULL;
	EGLImage egl_image;
	uint32_t drm_format;

	if (!init_egl_image_target_texture_2d_ext())
		return NULL;

	drm_format = gs_format_to_drm_format(color_format);
	if (drm_format == DRM_FORMAT_INVALID) {
		blog(LOG_ERROR, "Invalid or unsupported image format");
		return NULL;
	}

	egl_image = create_dmabuf_egl_image(egl_display, width, height,
					    drm_format, n_planes, fds, strides,
					    offsets, modifiers);
	if (egl_image == EGL_NO_IMAGE) {
		blog(LOG_ERROR, "Cannot create EGLImage: %s",
		     gl_egl_error_to_string(eglGetError()));
		return NULL;
	}

	texture = gs_texture_create(width, height, color_format, 1, NULL,
				    GS_DYNAMIC);
	const GLuint gltex = *(GLuint *)gs_texture_get_obj(texture);

	glBindTexture(GL_TEXTURE_2D, gltex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image);

	glBindTexture(GL_TEXTURE_2D, 0);
	eglDestroyImage(egl_display, egl_image);

	return texture;
}

const char *gl_egl_error_to_string(EGLint error_number)
{
	switch (error_number) {
	case EGL_SUCCESS:
		return "The last function succeeded without error.";
		break;
	case EGL_NOT_INITIALIZED:
		return "EGL is not initialized, or could not be initialized, for the specified EGL display connection.";
		break;
	case EGL_BAD_ACCESS:
		return "EGL cannot access a requested resource (for example a context is bound in another thread).";
		break;
	case EGL_BAD_ALLOC:
		return "EGL failed to allocate resources for the requested operation.";
		break;
	case EGL_BAD_ATTRIBUTE:
		return "An unrecognized attribute or attribute value was passed in the attribute list.";
		break;
	case EGL_BAD_CONTEXT:
		return "An EGLContext argument does not name a valid EGL rendering context.";
		break;
	case EGL_BAD_CONFIG:
		return "An EGLConfig argument does not name a valid EGL frame buffer configuration.";
		break;
	case EGL_BAD_CURRENT_SURFACE:
		return "The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid.";
		break;
	case EGL_BAD_DISPLAY:
		return "An EGLDisplay argument does not name a valid EGL display connection.";
		break;
	case EGL_BAD_SURFACE:
		return "An EGLSurface argument does not name a valid surface (window, pixel buffer or pixmap) configured for GL rendering.";
		break;
	case EGL_BAD_MATCH:
		return "Arguments are inconsistent (for example, a valid context requires buffers not supplied by a valid surface).";
		break;
	case EGL_BAD_PARAMETER:
		return "One or more argument values are invalid.";
		break;
	case EGL_BAD_NATIVE_PIXMAP:
		return "A NativePixmapType argument does not refer to a valid native pixmap.";
		break;
	case EGL_BAD_NATIVE_WINDOW:
		return "A NativeWindowType argument does not refer to a valid native window.";
		break;
	case EGL_CONTEXT_LOST:
		return "A power management event has occurred. The application must destroy all contexts and reinitialise OpenGL ES state and objects to continue rendering. ";
		break;
	default:
		return "Unknown error";
		break;
	}
}
