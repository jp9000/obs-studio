#pragma once

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef signed char GLbyte;
typedef short GLshort;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned long GLulong;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;
typedef ptrdiff_t GLintptrARB;
typedef ptrdiff_t GLsizeiptrARB;

#define GL_FRONT 0x0404
#define GL_BACK 0x0405

#define GL_INVALID_OPERATION 0x0502

#define GL_UNSIGNED_BYTE 0x1401

#define GL_RGB 0x1907
#define GL_RGBA 0x1908

#define GL_BGR 0x80E0
#define GL_BGRA 0x80E1

#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601

#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_BUFFER_ACCESS 0x88BB
#define GL_BUFFER_MAPPED 0x88BC
#define GL_BUFFER_MAP_POINTER 0x88BD
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_STATIC_DRAW 0x88E4
#define GL_STATIC_READ 0x88E5
#define GL_STATIC_COPY 0x88E6
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING 0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF

#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6

#define WGL_ACCESS_READ_ONLY_NV 0x0000
#define WGL_ACCESS_READ_WRITE_NV 0x0001
#define WGL_ACCESS_WRITE_DISCARD_NV 0x0002

#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1

typedef void(WINAPI *GLTEXIMAGE2DPROC)(GLenum target, GLint level,
				       GLint internal_format, GLsizei width,
				       GLsizei height, GLint border,
				       GLenum format, GLenum type,
				       const GLvoid *data);
typedef void(WINAPI *GLGETTEXIMAGEPROC)(GLenum target, GLint level,
					GLenum format, GLenum type,
					GLvoid *img);
typedef void(WINAPI *GLREADBUFFERPROC)(GLenum);
typedef void(WINAPI *GLDRAWBUFFERPROC)(GLenum mode);
typedef void(WINAPI *GLGETINTEGERVPROC)(GLenum pname, GLint *params);
typedef GLenum(WINAPI *GLGETERRORPROC)();
typedef BOOL(WINAPI *WGLSWAPLAYERBUFFERSPROC)(HDC, UINT);
typedef BOOL(WINAPI *WGLSWAPBUFFERSPROC)(HDC);
typedef BOOL(WINAPI *WGLDELETECONTEXTPROC)(HGLRC);
typedef PROC(WINAPI *WGLGETPROCADDRESSPROC)(LPCSTR);
typedef BOOL(WINAPI *WGLMAKECURRENTPROC)(HDC, HGLRC);
typedef HDC(WINAPI *WGLGETCURRENTDCPROC)();
typedef HGLRC(WINAPI *WGLGETCURRENTCONTEXTPROC)();
typedef HGLRC(WINAPI *WGLCREATECONTEXTPROC)(HDC);
typedef void(WINAPI *GLBUFFERDATAARBPROC)(GLenum target, GLsizeiptrARB size,
					  const GLvoid *data, GLenum usage);
typedef void(WINAPI *GLDELETEBUFFERSARBPROC)(GLsizei n, const GLuint *buffers);
typedef void(WINAPI *GLDELETETEXTURESPROC)(GLsizei n, const GLuint *buffers);
typedef void(WINAPI *GLGENBUFFERSARBPROC)(GLsizei n, GLuint *buffers);
typedef void(WINAPI *GLGENTEXTURESPROC)(GLsizei n, GLuint *textures);
typedef GLvoid *(WINAPI *GLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef GLboolean(WINAPI *GLUNMAPBUFFERPROC)(GLenum target);
typedef void(WINAPI *GLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void(WINAPI *GLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void(WINAPI *GLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void(WINAPI *GLDELETEFRAMEBUFFERSPROC)(GLsizei n, GLuint *framebuffers);
typedef void(WINAPI *GLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void(WINAPI *GLBLITFRAMEBUFFERPROC)(GLint srcX0, GLint srcY0,
					    GLint srcX1, GLint srcY1,
					    GLint dstX0, GLint dstY0,
					    GLint dstX1, GLint dstY1,
					    GLbitfield mask, GLenum filter);
typedef void(WINAPI *GLFRAMEBUFFERTEXTURE2DPROC)(GLenum target,
						 GLenum attachment,
						 GLenum textarget,
						 GLuint texture, GLint level);
typedef BOOL(WINAPI *WGLSETRESOURCESHAREHANDLENVPROC)(void *, HANDLE);
typedef HANDLE(WINAPI *WGLDXOPENDEVICENVPROC)(void *);
typedef BOOL(WINAPI *WGLDXCLOSEDEVICENVPROC)(HANDLE);
typedef HANDLE(WINAPI *WGLDXREGISTEROBJECTNVPROC)(HANDLE, void *, GLuint,
						  GLenum, GLenum);
typedef BOOL(WINAPI *WGLDXUNREGISTEROBJECTNVPROC)(HANDLE, HANDLE);
typedef BOOL(WINAPI *WGLDXOBJECTACCESSNVPROC)(HANDLE, GLenum);
typedef BOOL(WINAPI *WGLDXLOCKOBJECTSNVPROC)(HANDLE, GLint, HANDLE *);
typedef BOOL(WINAPI *WGLDXUNLOCKOBJECTSNVPROC)(HANDLE, GLint, HANDLE *);

static GLTEXIMAGE2DPROC glTexImage2D = NULL;
static GLGETTEXIMAGEPROC glGetTexImage = NULL;
static GLREADBUFFERPROC glReadBuffer = NULL;
static GLDRAWBUFFERPROC glDrawBuffer = NULL;
static GLGETINTEGERVPROC glGetIntegerv = NULL;
static GLGETERRORPROC glGetError = NULL;
static WGLGETPROCADDRESSPROC jimglGetProcAddress = NULL;
static WGLMAKECURRENTPROC jimglMakeCurrent = NULL;
static WGLGETCURRENTDCPROC jimglGetCurrentDC = NULL;
static WGLGETCURRENTCONTEXTPROC jimglGetCurrentContext = NULL;
static GLBUFFERDATAARBPROC glBufferData = NULL;
static GLDELETEBUFFERSARBPROC glDeleteBuffers = NULL;
static GLDELETETEXTURESPROC glDeleteTextures = NULL;
static GLGENBUFFERSARBPROC glGenBuffers = NULL;
static GLGENTEXTURESPROC glGenTextures = NULL;
static GLMAPBUFFERPROC glMapBuffer = NULL;
static GLUNMAPBUFFERPROC glUnmapBuffer = NULL;
static GLBINDBUFFERPROC glBindBuffer = NULL;
static GLBINDTEXTUREPROC glBindTexture = NULL;
static GLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
static GLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;
static GLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
static GLBLITFRAMEBUFFERPROC glBlitFramebuffer = NULL;
static GLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;

static WGLSETRESOURCESHAREHANDLENVPROC jimglDXSetResourceShareHandleNV = NULL;
static WGLDXOPENDEVICENVPROC jimglDXOpenDeviceNV = NULL;
static WGLDXCLOSEDEVICENVPROC jimglDXCloseDeviceNV = NULL;
static WGLDXREGISTEROBJECTNVPROC jimglDXRegisterObjectNV = NULL;
static WGLDXUNREGISTEROBJECTNVPROC jimglDXUnregisterObjectNV = NULL;
static WGLDXOBJECTACCESSNVPROC jimglDXObjectAccessNV = NULL;
static WGLDXLOCKOBJECTSNVPROC jimglDXLockObjectsNV = NULL;
static WGLDXUNLOCKOBJECTSNVPROC jimglDXUnlockObjectsNV = NULL;
