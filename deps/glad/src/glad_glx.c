#include <string.h>
#include <glad/glad_glx.h>

static void* get_proc(const char *namez);

#ifdef _WIN32
#include <windows.h>
static HMODULE libGL;

typedef void* (APIENTRYP PFNWGLGETPROCADDRESSPROC_PRIVATE)(const char*);
PFNWGLGETPROCADDRESSPROC_PRIVATE gladGetProcAddressPtr;

static
int open_gl(void) {
    libGL = LoadLibraryA("opengl32.dll");
    if(libGL != NULL) {
        gladGetProcAddressPtr = (PFNWGLGETPROCADDRESSPROC_PRIVATE)GetProcAddress(
                libGL, "wglGetProcAddress");
        return gladGetProcAddressPtr != NULL;
    }

    return 0;
}

static
void close_gl(void) {
    if(libGL != NULL) {
        FreeLibrary(libGL);
        libGL = NULL;
    }
}
#else
#include <dlfcn.h>
static void* libGL;

#ifndef __APPLE__
typedef void* (APIENTRYP PFNGLXGETPROCADDRESSPROC_PRIVATE)(const char*);
extern PFNGLXGETPROCADDRESSPROC_PRIVATE gladGetProcAddressPtr;
#endif

static
int open_gl(void) {
#ifdef __APPLE__
    static const char *NAMES[] = {
        "../Frameworks/OpenGL.framework/OpenGL",
        "/Library/Frameworks/OpenGL.framework/OpenGL",
        "/System/Library/Frameworks/OpenGL.framework/OpenGL",
        "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL"
    };
#else
    static const char *NAMES[] = {"libGL.so.1", "libGL.so"};
#endif

    unsigned int index = 0;
    for(index = 0; index < (sizeof(NAMES) / sizeof(NAMES[0])); index++) {
        libGL = dlopen(NAMES[index], RTLD_NOW | RTLD_GLOBAL);

        if(libGL != NULL) {
#ifdef __APPLE__
            return 1;
#else
            gladGetProcAddressPtr = (PFNGLXGETPROCADDRESSPROC_PRIVATE)dlsym(libGL,
                "glXGetProcAddressARB");
            return gladGetProcAddressPtr != NULL;
#endif
        }
    }

    return 0;
}

static
void close_gl() {
    if(libGL != NULL) {
        dlclose(libGL);
        libGL = NULL;
    }
}
#endif

static
void* get_proc(const char *namez) {
    void* result = NULL;
    if(libGL == NULL) return NULL;

#ifndef __APPLE__
    if(gladGetProcAddressPtr != NULL) {
        result = gladGetProcAddressPtr(namez);
    }
#endif
    if(result == NULL) {
#ifdef _WIN32
        result = (void*)GetProcAddress(libGL, namez);
#else
        result = dlsym(libGL, namez);
#endif
    }

    return result;
}

int gladLoadGLX(Display *dpy, int screen) {
    if(open_gl()) {
        gladLoadGLXLoader((GLADloadproc)get_proc, dpy, screen);
        close_gl();
        return 1;
    }

    return 0;
}

static Display *GLADGLXDisplay = 0;
static int GLADGLXscreen = 0;

static int has_ext(const char *ext) {
    const char *terminator;
    const char *loc;
    const char *extensions;

    if(!GLAD_GLX_VERSION_1_1)
        return 0;

    extensions = glXQueryExtensionsString(GLADGLXDisplay, GLADGLXscreen);

    if(extensions == NULL || ext == NULL)
        return 0;

    while(1) {
        loc = strstr(extensions, ext);
        if(loc == NULL)
            break;

        terminator = loc + strlen(ext);
        if((loc == extensions || *(loc - 1) == ' ') &&
            (*terminator == ' ' || *terminator == '\0'))
        {
            return 1;
        }
        extensions = terminator;
    }

    return 0;
}

int GLAD_GLX_VERSION_1_0;
int GLAD_GLX_VERSION_1_1;
int GLAD_GLX_VERSION_1_2;
int GLAD_GLX_VERSION_1_3;
int GLAD_GLX_VERSION_1_4;
PFNGLXGETSELECTEDEVENTPROC glad_glXGetSelectedEvent;
PFNGLXQUERYEXTENSIONPROC glad_glXQueryExtension;
PFNGLXMAKECURRENTPROC glad_glXMakeCurrent;
PFNGLXSELECTEVENTPROC glad_glXSelectEvent;
PFNGLXCREATECONTEXTPROC glad_glXCreateContext;
PFNGLXCREATEGLXPIXMAPPROC glad_glXCreateGLXPixmap;
PFNGLXQUERYVERSIONPROC glad_glXQueryVersion;
PFNGLXGETCURRENTREADDRAWABLEPROC glad_glXGetCurrentReadDrawable;
PFNGLXDESTROYPIXMAPPROC glad_glXDestroyPixmap;
PFNGLXGETCURRENTCONTEXTPROC glad_glXGetCurrentContext;
PFNGLXGETPROCADDRESSPROC glad_glXGetProcAddress;
PFNGLXWAITGLPROC glad_glXWaitGL;
PFNGLXISDIRECTPROC glad_glXIsDirect;
PFNGLXDESTROYWINDOWPROC glad_glXDestroyWindow;
PFNGLXCREATEWINDOWPROC glad_glXCreateWindow;
PFNGLXCOPYCONTEXTPROC glad_glXCopyContext;
PFNGLXCREATEPBUFFERPROC glad_glXCreatePbuffer;
PFNGLXSWAPBUFFERSPROC glad_glXSwapBuffers;
PFNGLXGETCURRENTDISPLAYPROC glad_glXGetCurrentDisplay;
PFNGLXGETCURRENTDRAWABLEPROC glad_glXGetCurrentDrawable;
PFNGLXQUERYCONTEXTPROC glad_glXQueryContext;
PFNGLXCHOOSEVISUALPROC glad_glXChooseVisual;
PFNGLXQUERYSERVERSTRINGPROC glad_glXQueryServerString;
PFNGLXDESTROYCONTEXTPROC glad_glXDestroyContext;
PFNGLXDESTROYGLXPIXMAPPROC glad_glXDestroyGLXPixmap;
PFNGLXGETFBCONFIGATTRIBPROC glad_glXGetFBConfigAttrib;
PFNGLXUSEXFONTPROC glad_glXUseXFont;
PFNGLXDESTROYPBUFFERPROC glad_glXDestroyPbuffer;
PFNGLXCHOOSEFBCONFIGPROC glad_glXChooseFBConfig;
PFNGLXCREATENEWCONTEXTPROC glad_glXCreateNewContext;
PFNGLXMAKECONTEXTCURRENTPROC glad_glXMakeContextCurrent;
PFNGLXGETCONFIGPROC glad_glXGetConfig;
PFNGLXGETFBCONFIGSPROC glad_glXGetFBConfigs;
PFNGLXCREATEPIXMAPPROC glad_glXCreatePixmap;
PFNGLXWAITXPROC glad_glXWaitX;
PFNGLXGETVISUALFROMFBCONFIGPROC glad_glXGetVisualFromFBConfig;
PFNGLXQUERYDRAWABLEPROC glad_glXQueryDrawable;
PFNGLXQUERYEXTENSIONSSTRINGPROC glad_glXQueryExtensionsString;
PFNGLXGETCLIENTSTRINGPROC glad_glXGetClientString;
int GLAD_GLX_ARB_framebuffer_sRGB;
int GLAD_GLX_EXT_import_context;
int GLAD_GLX_NV_multisample_coverage;
int GLAD_GLX_SGIS_shared_multisample;
int GLAD_GLX_SGIX_pbuffer;
int GLAD_GLX_NV_swap_group;
int GLAD_GLX_ARB_fbconfig_float;
int GLAD_GLX_SGIX_hyperpipe;
int GLAD_GLX_ARB_robustness_share_group_isolation;
int GLAD_GLX_INTEL_swap_event;
int GLAD_GLX_SGIX_video_resize;
int GLAD_GLX_EXT_create_context_es2_profile;
int GLAD_GLX_ARB_robustness_application_isolation;
int GLAD_GLX_NV_copy_image;
int GLAD_GLX_OML_sync_control;
int GLAD_GLX_EXT_framebuffer_sRGB;
int GLAD_GLX_SGI_make_current_read;
int GLAD_GLX_MESA_swap_control;
int GLAD_GLX_SGI_swap_control;
int GLAD_GLX_EXT_fbconfig_packed_float;
int GLAD_GLX_EXT_buffer_age;
int GLAD_GLX_3DFX_multisample;
int GLAD_GLX_EXT_visual_info;
int GLAD_GLX_SGI_video_sync;
int GLAD_GLX_MESA_agp_offset;
int GLAD_GLX_SGIS_multisample;
int GLAD_GLX_MESA_set_3dfx_mode;
int GLAD_GLX_EXT_texture_from_pixmap;
int GLAD_GLX_NV_video_capture;
int GLAD_GLX_ARB_multisample;
int GLAD_GLX_NV_delay_before_swap;
int GLAD_GLX_SGIX_swap_group;
int GLAD_GLX_EXT_swap_control;
int GLAD_GLX_SGIX_video_source;
int GLAD_GLX_MESA_query_renderer;
int GLAD_GLX_ARB_create_context;
int GLAD_GLX_EXT_create_context_es_profile;
int GLAD_GLX_SGIX_fbconfig;
int GLAD_GLX_MESA_pixmap_colormap;
int GLAD_GLX_SGIX_visual_select_group;
int GLAD_GLX_NV_video_output;
int GLAD_GLX_SGIS_blended_overlay;
int GLAD_GLX_SGIX_dmbuffer;
int GLAD_GLX_ARB_create_context_robustness;
int GLAD_GLX_SGIX_swap_barrier;
int GLAD_GLX_EXT_swap_control_tear;
int GLAD_GLX_MESA_release_buffers;
int GLAD_GLX_EXT_visual_rating;
int GLAD_GLX_MESA_copy_sub_buffer;
int GLAD_GLX_SGI_cushion;
int GLAD_GLX_NV_float_buffer;
int GLAD_GLX_OML_swap_method;
int GLAD_GLX_NV_present_video;
int GLAD_GLX_SUN_get_transparent_index;
int GLAD_GLX_AMD_gpu_association;
int GLAD_GLX_ARB_create_context_profile;
int GLAD_GLX_ARB_get_proc_address;
int GLAD_GLX_ARB_vertex_buffer_object;
PFNGLXGETCURRENTDISPLAYEXTPROC glad_glXGetCurrentDisplayEXT;
PFNGLXQUERYCONTEXTINFOEXTPROC glad_glXQueryContextInfoEXT;
PFNGLXGETCONTEXTIDEXTPROC glad_glXGetContextIDEXT;
PFNGLXIMPORTCONTEXTEXTPROC glad_glXImportContextEXT;
PFNGLXFREECONTEXTEXTPROC glad_glXFreeContextEXT;
PFNGLXCREATEGLXPBUFFERSGIXPROC glad_glXCreateGLXPbufferSGIX;
PFNGLXDESTROYGLXPBUFFERSGIXPROC glad_glXDestroyGLXPbufferSGIX;
PFNGLXQUERYGLXPBUFFERSGIXPROC glad_glXQueryGLXPbufferSGIX;
PFNGLXSELECTEVENTSGIXPROC glad_glXSelectEventSGIX;
PFNGLXGETSELECTEDEVENTSGIXPROC glad_glXGetSelectedEventSGIX;
PFNGLXJOINSWAPGROUPNVPROC glad_glXJoinSwapGroupNV;
PFNGLXBINDSWAPBARRIERNVPROC glad_glXBindSwapBarrierNV;
PFNGLXQUERYSWAPGROUPNVPROC glad_glXQuerySwapGroupNV;
PFNGLXQUERYMAXSWAPGROUPSNVPROC glad_glXQueryMaxSwapGroupsNV;
PFNGLXQUERYFRAMECOUNTNVPROC glad_glXQueryFrameCountNV;
PFNGLXRESETFRAMECOUNTNVPROC glad_glXResetFrameCountNV;
PFNGLXQUERYHYPERPIPENETWORKSGIXPROC glad_glXQueryHyperpipeNetworkSGIX;
PFNGLXHYPERPIPECONFIGSGIXPROC glad_glXHyperpipeConfigSGIX;
PFNGLXQUERYHYPERPIPECONFIGSGIXPROC glad_glXQueryHyperpipeConfigSGIX;
PFNGLXDESTROYHYPERPIPECONFIGSGIXPROC glad_glXDestroyHyperpipeConfigSGIX;
PFNGLXBINDHYPERPIPESGIXPROC glad_glXBindHyperpipeSGIX;
PFNGLXQUERYHYPERPIPEBESTATTRIBSGIXPROC glad_glXQueryHyperpipeBestAttribSGIX;
PFNGLXHYPERPIPEATTRIBSGIXPROC glad_glXHyperpipeAttribSGIX;
PFNGLXQUERYHYPERPIPEATTRIBSGIXPROC glad_glXQueryHyperpipeAttribSGIX;
PFNGLXBINDCHANNELTOWINDOWSGIXPROC glad_glXBindChannelToWindowSGIX;
PFNGLXCHANNELRECTSGIXPROC glad_glXChannelRectSGIX;
PFNGLXQUERYCHANNELRECTSGIXPROC glad_glXQueryChannelRectSGIX;
PFNGLXQUERYCHANNELDELTASSGIXPROC glad_glXQueryChannelDeltasSGIX;
PFNGLXCHANNELRECTSYNCSGIXPROC glad_glXChannelRectSyncSGIX;
PFNGLXCOPYIMAGESUBDATANVPROC glad_glXCopyImageSubDataNV;
PFNGLXGETSYNCVALUESOMLPROC glad_glXGetSyncValuesOML;
PFNGLXGETMSCRATEOMLPROC glad_glXGetMscRateOML;
PFNGLXSWAPBUFFERSMSCOMLPROC glad_glXSwapBuffersMscOML;
PFNGLXWAITFORMSCOMLPROC glad_glXWaitForMscOML;
PFNGLXWAITFORSBCOMLPROC glad_glXWaitForSbcOML;
PFNGLXMAKECURRENTREADSGIPROC glad_glXMakeCurrentReadSGI;
PFNGLXGETCURRENTREADDRAWABLESGIPROC glad_glXGetCurrentReadDrawableSGI;
PFNGLXSWAPINTERVALMESAPROC glad_glXSwapIntervalMESA;
PFNGLXSWAPINTERVALSGIPROC glad_glXSwapIntervalSGI;
PFNGLXGETVIDEOSYNCSGIPROC glad_glXGetVideoSyncSGI;
PFNGLXWAITVIDEOSYNCSGIPROC glad_glXWaitVideoSyncSGI;
PFNGLXGETAGPOFFSETMESAPROC glad_glXGetAGPOffsetMESA;
PFNGLXSET3DFXMODEMESAPROC glad_glXSet3DfxModeMESA;
PFNGLXBINDTEXIMAGEEXTPROC glad_glXBindTexImageEXT;
PFNGLXRELEASETEXIMAGEEXTPROC glad_glXReleaseTexImageEXT;
PFNGLXBINDVIDEOCAPTUREDEVICENVPROC glad_glXBindVideoCaptureDeviceNV;
PFNGLXENUMERATEVIDEOCAPTUREDEVICESNVPROC glad_glXEnumerateVideoCaptureDevicesNV;
PFNGLXLOCKVIDEOCAPTUREDEVICENVPROC glad_glXLockVideoCaptureDeviceNV;
PFNGLXQUERYVIDEOCAPTUREDEVICENVPROC glad_glXQueryVideoCaptureDeviceNV;
PFNGLXRELEASEVIDEOCAPTUREDEVICENVPROC glad_glXReleaseVideoCaptureDeviceNV;
PFNGLXDELAYBEFORESWAPNVPROC glad_glXDelayBeforeSwapNV;
PFNGLXJOINSWAPGROUPSGIXPROC glad_glXJoinSwapGroupSGIX;
PFNGLXSWAPINTERVALEXTPROC glad_glXSwapIntervalEXT;
#ifdef _VL_H_
PFNGLXCREATEGLXVIDEOSOURCESGIXPROC glad_glXCreateGLXVideoSourceSGIX;
PFNGLXDESTROYGLXVIDEOSOURCESGIXPROC glad_glXDestroyGLXVideoSourceSGIX;
#endif
PFNGLXQUERYCURRENTRENDERERINTEGERMESAPROC glad_glXQueryCurrentRendererIntegerMESA;
PFNGLXQUERYCURRENTRENDERERSTRINGMESAPROC glad_glXQueryCurrentRendererStringMESA;
PFNGLXQUERYRENDERERINTEGERMESAPROC glad_glXQueryRendererIntegerMESA;
PFNGLXQUERYRENDERERSTRINGMESAPROC glad_glXQueryRendererStringMESA;
PFNGLXCREATECONTEXTATTRIBSARBPROC glad_glXCreateContextAttribsARB;
PFNGLXGETFBCONFIGATTRIBSGIXPROC glad_glXGetFBConfigAttribSGIX;
PFNGLXCHOOSEFBCONFIGSGIXPROC glad_glXChooseFBConfigSGIX;
PFNGLXCREATEGLXPIXMAPWITHCONFIGSGIXPROC glad_glXCreateGLXPixmapWithConfigSGIX;
PFNGLXCREATECONTEXTWITHCONFIGSGIXPROC glad_glXCreateContextWithConfigSGIX;
PFNGLXGETVISUALFROMFBCONFIGSGIXPROC glad_glXGetVisualFromFBConfigSGIX;
PFNGLXGETFBCONFIGFROMVISUALSGIXPROC glad_glXGetFBConfigFromVisualSGIX;
PFNGLXCREATEGLXPIXMAPMESAPROC glad_glXCreateGLXPixmapMESA;
PFNGLXGETVIDEODEVICENVPROC glad_glXGetVideoDeviceNV;
PFNGLXRELEASEVIDEODEVICENVPROC glad_glXReleaseVideoDeviceNV;
PFNGLXBINDVIDEOIMAGENVPROC glad_glXBindVideoImageNV;
PFNGLXRELEASEVIDEOIMAGENVPROC glad_glXReleaseVideoImageNV;
PFNGLXSENDPBUFFERTOVIDEONVPROC glad_glXSendPbufferToVideoNV;
PFNGLXGETVIDEOINFONVPROC glad_glXGetVideoInfoNV;
#ifdef _DM_BUFFER_H_
PFNGLXASSOCIATEDMPBUFFERSGIXPROC glad_glXAssociateDMPbufferSGIX;
#endif
PFNGLXBINDSWAPBARRIERSGIXPROC glad_glXBindSwapBarrierSGIX;
PFNGLXQUERYMAXSWAPBARRIERSSGIXPROC glad_glXQueryMaxSwapBarriersSGIX;
PFNGLXRELEASEBUFFERSMESAPROC glad_glXReleaseBuffersMESA;
PFNGLXCOPYSUBBUFFERMESAPROC glad_glXCopySubBufferMESA;
PFNGLXCUSHIONSGIPROC glad_glXCushionSGI;
PFNGLXENUMERATEVIDEODEVICESNVPROC glad_glXEnumerateVideoDevicesNV;
PFNGLXBINDVIDEODEVICENVPROC glad_glXBindVideoDeviceNV;
PFNGLXGETTRANSPARENTINDEXSUNPROC glad_glXGetTransparentIndexSUN;
PFNGLXGETPROCADDRESSARBPROC glad_glXGetProcAddressARB;
static void load_GLX_VERSION_1_0(GLADloadproc load) {
	if(!GLAD_GLX_VERSION_1_0) return;
	glad_glXChooseVisual = (PFNGLXCHOOSEVISUALPROC)load("glXChooseVisual");
	glad_glXCreateContext = (PFNGLXCREATECONTEXTPROC)load("glXCreateContext");
	glad_glXDestroyContext = (PFNGLXDESTROYCONTEXTPROC)load("glXDestroyContext");
	glad_glXMakeCurrent = (PFNGLXMAKECURRENTPROC)load("glXMakeCurrent");
	glad_glXCopyContext = (PFNGLXCOPYCONTEXTPROC)load("glXCopyContext");
	glad_glXSwapBuffers = (PFNGLXSWAPBUFFERSPROC)load("glXSwapBuffers");
	glad_glXCreateGLXPixmap = (PFNGLXCREATEGLXPIXMAPPROC)load("glXCreateGLXPixmap");
	glad_glXDestroyGLXPixmap = (PFNGLXDESTROYGLXPIXMAPPROC)load("glXDestroyGLXPixmap");
	glad_glXQueryExtension = (PFNGLXQUERYEXTENSIONPROC)load("glXQueryExtension");
	glad_glXQueryVersion = (PFNGLXQUERYVERSIONPROC)load("glXQueryVersion");
	glad_glXIsDirect = (PFNGLXISDIRECTPROC)load("glXIsDirect");
	glad_glXGetConfig = (PFNGLXGETCONFIGPROC)load("glXGetConfig");
	glad_glXGetCurrentContext = (PFNGLXGETCURRENTCONTEXTPROC)load("glXGetCurrentContext");
	glad_glXGetCurrentDrawable = (PFNGLXGETCURRENTDRAWABLEPROC)load("glXGetCurrentDrawable");
	glad_glXWaitGL = (PFNGLXWAITGLPROC)load("glXWaitGL");
	glad_glXWaitX = (PFNGLXWAITXPROC)load("glXWaitX");
	glad_glXUseXFont = (PFNGLXUSEXFONTPROC)load("glXUseXFont");
}
static void load_GLX_VERSION_1_1(GLADloadproc load) {
	if(!GLAD_GLX_VERSION_1_1) return;
	glad_glXQueryExtensionsString = (PFNGLXQUERYEXTENSIONSSTRINGPROC)load("glXQueryExtensionsString");
	glad_glXQueryServerString = (PFNGLXQUERYSERVERSTRINGPROC)load("glXQueryServerString");
	glad_glXGetClientString = (PFNGLXGETCLIENTSTRINGPROC)load("glXGetClientString");
}
static void load_GLX_VERSION_1_2(GLADloadproc load) {
	if(!GLAD_GLX_VERSION_1_2) return;
	glad_glXGetCurrentDisplay = (PFNGLXGETCURRENTDISPLAYPROC)load("glXGetCurrentDisplay");
}
static void load_GLX_VERSION_1_3(GLADloadproc load) {
	if(!GLAD_GLX_VERSION_1_3) return;
	glad_glXGetFBConfigs = (PFNGLXGETFBCONFIGSPROC)load("glXGetFBConfigs");
	glad_glXChooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC)load("glXChooseFBConfig");
	glad_glXGetFBConfigAttrib = (PFNGLXGETFBCONFIGATTRIBPROC)load("glXGetFBConfigAttrib");
	glad_glXGetVisualFromFBConfig = (PFNGLXGETVISUALFROMFBCONFIGPROC)load("glXGetVisualFromFBConfig");
	glad_glXCreateWindow = (PFNGLXCREATEWINDOWPROC)load("glXCreateWindow");
	glad_glXDestroyWindow = (PFNGLXDESTROYWINDOWPROC)load("glXDestroyWindow");
	glad_glXCreatePixmap = (PFNGLXCREATEPIXMAPPROC)load("glXCreatePixmap");
	glad_glXDestroyPixmap = (PFNGLXDESTROYPIXMAPPROC)load("glXDestroyPixmap");
	glad_glXCreatePbuffer = (PFNGLXCREATEPBUFFERPROC)load("glXCreatePbuffer");
	glad_glXDestroyPbuffer = (PFNGLXDESTROYPBUFFERPROC)load("glXDestroyPbuffer");
	glad_glXQueryDrawable = (PFNGLXQUERYDRAWABLEPROC)load("glXQueryDrawable");
	glad_glXCreateNewContext = (PFNGLXCREATENEWCONTEXTPROC)load("glXCreateNewContext");
	glad_glXMakeContextCurrent = (PFNGLXMAKECONTEXTCURRENTPROC)load("glXMakeContextCurrent");
	glad_glXGetCurrentReadDrawable = (PFNGLXGETCURRENTREADDRAWABLEPROC)load("glXGetCurrentReadDrawable");
	glad_glXQueryContext = (PFNGLXQUERYCONTEXTPROC)load("glXQueryContext");
	glad_glXSelectEvent = (PFNGLXSELECTEVENTPROC)load("glXSelectEvent");
	glad_glXGetSelectedEvent = (PFNGLXGETSELECTEDEVENTPROC)load("glXGetSelectedEvent");
}
static void load_GLX_VERSION_1_4(GLADloadproc load) {
	if(!GLAD_GLX_VERSION_1_4) return;
	glad_glXGetProcAddress = (PFNGLXGETPROCADDRESSPROC)load("glXGetProcAddress");
}
static void load_GLX_EXT_import_context(GLADloadproc load) {
	if(!GLAD_GLX_EXT_import_context) return;
	glad_glXGetCurrentDisplayEXT = (PFNGLXGETCURRENTDISPLAYEXTPROC)load("glXGetCurrentDisplayEXT");
	glad_glXQueryContextInfoEXT = (PFNGLXQUERYCONTEXTINFOEXTPROC)load("glXQueryContextInfoEXT");
	glad_glXGetContextIDEXT = (PFNGLXGETCONTEXTIDEXTPROC)load("glXGetContextIDEXT");
	glad_glXImportContextEXT = (PFNGLXIMPORTCONTEXTEXTPROC)load("glXImportContextEXT");
	glad_glXFreeContextEXT = (PFNGLXFREECONTEXTEXTPROC)load("glXFreeContextEXT");
}
static void load_GLX_SGIX_pbuffer(GLADloadproc load) {
	if(!GLAD_GLX_SGIX_pbuffer) return;
	glad_glXCreateGLXPbufferSGIX = (PFNGLXCREATEGLXPBUFFERSGIXPROC)load("glXCreateGLXPbufferSGIX");
	glad_glXDestroyGLXPbufferSGIX = (PFNGLXDESTROYGLXPBUFFERSGIXPROC)load("glXDestroyGLXPbufferSGIX");
	glad_glXQueryGLXPbufferSGIX = (PFNGLXQUERYGLXPBUFFERSGIXPROC)load("glXQueryGLXPbufferSGIX");
	glad_glXSelectEventSGIX = (PFNGLXSELECTEVENTSGIXPROC)load("glXSelectEventSGIX");
	glad_glXGetSelectedEventSGIX = (PFNGLXGETSELECTEDEVENTSGIXPROC)load("glXGetSelectedEventSGIX");
}
static void load_GLX_NV_swap_group(GLADloadproc load) {
	if(!GLAD_GLX_NV_swap_group) return;
	glad_glXJoinSwapGroupNV = (PFNGLXJOINSWAPGROUPNVPROC)load("glXJoinSwapGroupNV");
	glad_glXBindSwapBarrierNV = (PFNGLXBINDSWAPBARRIERNVPROC)load("glXBindSwapBarrierNV");
	glad_glXQuerySwapGroupNV = (PFNGLXQUERYSWAPGROUPNVPROC)load("glXQuerySwapGroupNV");
	glad_glXQueryMaxSwapGroupsNV = (PFNGLXQUERYMAXSWAPGROUPSNVPROC)load("glXQueryMaxSwapGroupsNV");
	glad_glXQueryFrameCountNV = (PFNGLXQUERYFRAMECOUNTNVPROC)load("glXQueryFrameCountNV");
	glad_glXResetFrameCountNV = (PFNGLXRESETFRAMECOUNTNVPROC)load("glXResetFrameCountNV");
}
static void load_GLX_SGIX_hyperpipe(GLADloadproc load) {
	if(!GLAD_GLX_SGIX_hyperpipe) return;
	glad_glXQueryHyperpipeNetworkSGIX = (PFNGLXQUERYHYPERPIPENETWORKSGIXPROC)load("glXQueryHyperpipeNetworkSGIX");
	glad_glXHyperpipeConfigSGIX = (PFNGLXHYPERPIPECONFIGSGIXPROC)load("glXHyperpipeConfigSGIX");
	glad_glXQueryHyperpipeConfigSGIX = (PFNGLXQUERYHYPERPIPECONFIGSGIXPROC)load("glXQueryHyperpipeConfigSGIX");
	glad_glXDestroyHyperpipeConfigSGIX = (PFNGLXDESTROYHYPERPIPECONFIGSGIXPROC)load("glXDestroyHyperpipeConfigSGIX");
	glad_glXBindHyperpipeSGIX = (PFNGLXBINDHYPERPIPESGIXPROC)load("glXBindHyperpipeSGIX");
	glad_glXQueryHyperpipeBestAttribSGIX = (PFNGLXQUERYHYPERPIPEBESTATTRIBSGIXPROC)load("glXQueryHyperpipeBestAttribSGIX");
	glad_glXHyperpipeAttribSGIX = (PFNGLXHYPERPIPEATTRIBSGIXPROC)load("glXHyperpipeAttribSGIX");
	glad_glXQueryHyperpipeAttribSGIX = (PFNGLXQUERYHYPERPIPEATTRIBSGIXPROC)load("glXQueryHyperpipeAttribSGIX");
}
static void load_GLX_SGIX_video_resize(GLADloadproc load) {
	if(!GLAD_GLX_SGIX_video_resize) return;
	glad_glXBindChannelToWindowSGIX = (PFNGLXBINDCHANNELTOWINDOWSGIXPROC)load("glXBindChannelToWindowSGIX");
	glad_glXChannelRectSGIX = (PFNGLXCHANNELRECTSGIXPROC)load("glXChannelRectSGIX");
	glad_glXQueryChannelRectSGIX = (PFNGLXQUERYCHANNELRECTSGIXPROC)load("glXQueryChannelRectSGIX");
	glad_glXQueryChannelDeltasSGIX = (PFNGLXQUERYCHANNELDELTASSGIXPROC)load("glXQueryChannelDeltasSGIX");
	glad_glXChannelRectSyncSGIX = (PFNGLXCHANNELRECTSYNCSGIXPROC)load("glXChannelRectSyncSGIX");
}
static void load_GLX_NV_copy_image(GLADloadproc load) {
	if(!GLAD_GLX_NV_copy_image) return;
	glad_glXCopyImageSubDataNV = (PFNGLXCOPYIMAGESUBDATANVPROC)load("glXCopyImageSubDataNV");
}
static void load_GLX_OML_sync_control(GLADloadproc load) {
	if(!GLAD_GLX_OML_sync_control) return;
	glad_glXGetSyncValuesOML = (PFNGLXGETSYNCVALUESOMLPROC)load("glXGetSyncValuesOML");
	glad_glXGetMscRateOML = (PFNGLXGETMSCRATEOMLPROC)load("glXGetMscRateOML");
	glad_glXSwapBuffersMscOML = (PFNGLXSWAPBUFFERSMSCOMLPROC)load("glXSwapBuffersMscOML");
	glad_glXWaitForMscOML = (PFNGLXWAITFORMSCOMLPROC)load("glXWaitForMscOML");
	glad_glXWaitForSbcOML = (PFNGLXWAITFORSBCOMLPROC)load("glXWaitForSbcOML");
}
static void load_GLX_SGI_make_current_read(GLADloadproc load) {
	if(!GLAD_GLX_SGI_make_current_read) return;
	glad_glXMakeCurrentReadSGI = (PFNGLXMAKECURRENTREADSGIPROC)load("glXMakeCurrentReadSGI");
	glad_glXGetCurrentReadDrawableSGI = (PFNGLXGETCURRENTREADDRAWABLESGIPROC)load("glXGetCurrentReadDrawableSGI");
}
static void load_GLX_MESA_swap_control(GLADloadproc load) {
	if(!GLAD_GLX_MESA_swap_control) return;
	glad_glXSwapIntervalMESA = (PFNGLXSWAPINTERVALMESAPROC)load("glXSwapIntervalMESA");
}
static void load_GLX_SGI_swap_control(GLADloadproc load) {
	if(!GLAD_GLX_SGI_swap_control) return;
	glad_glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)load("glXSwapIntervalSGI");
}
static void load_GLX_SGI_video_sync(GLADloadproc load) {
	if(!GLAD_GLX_SGI_video_sync) return;
	glad_glXGetVideoSyncSGI = (PFNGLXGETVIDEOSYNCSGIPROC)load("glXGetVideoSyncSGI");
	glad_glXWaitVideoSyncSGI = (PFNGLXWAITVIDEOSYNCSGIPROC)load("glXWaitVideoSyncSGI");
}
static void load_GLX_MESA_agp_offset(GLADloadproc load) {
	if(!GLAD_GLX_MESA_agp_offset) return;
	glad_glXGetAGPOffsetMESA = (PFNGLXGETAGPOFFSETMESAPROC)load("glXGetAGPOffsetMESA");
}
static void load_GLX_MESA_set_3dfx_mode(GLADloadproc load) {
	if(!GLAD_GLX_MESA_set_3dfx_mode) return;
	glad_glXSet3DfxModeMESA = (PFNGLXSET3DFXMODEMESAPROC)load("glXSet3DfxModeMESA");
}
static void load_GLX_EXT_texture_from_pixmap(GLADloadproc load) {
	if(!GLAD_GLX_EXT_texture_from_pixmap) return;
	glad_glXBindTexImageEXT = (PFNGLXBINDTEXIMAGEEXTPROC)load("glXBindTexImageEXT");
	glad_glXReleaseTexImageEXT = (PFNGLXRELEASETEXIMAGEEXTPROC)load("glXReleaseTexImageEXT");
}
static void load_GLX_NV_video_capture(GLADloadproc load) {
	if(!GLAD_GLX_NV_video_capture) return;
	glad_glXBindVideoCaptureDeviceNV = (PFNGLXBINDVIDEOCAPTUREDEVICENVPROC)load("glXBindVideoCaptureDeviceNV");
	glad_glXEnumerateVideoCaptureDevicesNV = (PFNGLXENUMERATEVIDEOCAPTUREDEVICESNVPROC)load("glXEnumerateVideoCaptureDevicesNV");
	glad_glXLockVideoCaptureDeviceNV = (PFNGLXLOCKVIDEOCAPTUREDEVICENVPROC)load("glXLockVideoCaptureDeviceNV");
	glad_glXQueryVideoCaptureDeviceNV = (PFNGLXQUERYVIDEOCAPTUREDEVICENVPROC)load("glXQueryVideoCaptureDeviceNV");
	glad_glXReleaseVideoCaptureDeviceNV = (PFNGLXRELEASEVIDEOCAPTUREDEVICENVPROC)load("glXReleaseVideoCaptureDeviceNV");
}
static void load_GLX_NV_delay_before_swap(GLADloadproc load) {
	if(!GLAD_GLX_NV_delay_before_swap) return;
	glad_glXDelayBeforeSwapNV = (PFNGLXDELAYBEFORESWAPNVPROC)load("glXDelayBeforeSwapNV");
}
static void load_GLX_SGIX_swap_group(GLADloadproc load) {
	if(!GLAD_GLX_SGIX_swap_group) return;
	glad_glXJoinSwapGroupSGIX = (PFNGLXJOINSWAPGROUPSGIXPROC)load("glXJoinSwapGroupSGIX");
}
static void load_GLX_EXT_swap_control(GLADloadproc load) {
	if(!GLAD_GLX_EXT_swap_control) return;
	glad_glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)load("glXSwapIntervalEXT");
}
static void load_GLX_SGIX_video_source(GLADloadproc load) {
	if(!GLAD_GLX_SGIX_video_source) return;
#ifdef _VL_H_
	glad_glXCreateGLXVideoSourceSGIX = (PFNGLXCREATEGLXVIDEOSOURCESGIXPROC)load("glXCreateGLXVideoSourceSGIX");
	glad_glXDestroyGLXVideoSourceSGIX = (PFNGLXDESTROYGLXVIDEOSOURCESGIXPROC)load("glXDestroyGLXVideoSourceSGIX");
#else
	(void)load;
#endif
}
static void load_GLX_MESA_query_renderer(GLADloadproc load) {
	if(!GLAD_GLX_MESA_query_renderer) return;
	glad_glXQueryCurrentRendererIntegerMESA = (PFNGLXQUERYCURRENTRENDERERINTEGERMESAPROC)load("glXQueryCurrentRendererIntegerMESA");
	glad_glXQueryCurrentRendererStringMESA = (PFNGLXQUERYCURRENTRENDERERSTRINGMESAPROC)load("glXQueryCurrentRendererStringMESA");
	glad_glXQueryRendererIntegerMESA = (PFNGLXQUERYRENDERERINTEGERMESAPROC)load("glXQueryRendererIntegerMESA");
	glad_glXQueryRendererStringMESA = (PFNGLXQUERYRENDERERSTRINGMESAPROC)load("glXQueryRendererStringMESA");
}
static void load_GLX_ARB_create_context(GLADloadproc load) {
	if(!GLAD_GLX_ARB_create_context) return;
	glad_glXCreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC)load("glXCreateContextAttribsARB");
}
static void load_GLX_SGIX_fbconfig(GLADloadproc load) {
	if(!GLAD_GLX_SGIX_fbconfig) return;
	glad_glXGetFBConfigAttribSGIX = (PFNGLXGETFBCONFIGATTRIBSGIXPROC)load("glXGetFBConfigAttribSGIX");
	glad_glXChooseFBConfigSGIX = (PFNGLXCHOOSEFBCONFIGSGIXPROC)load("glXChooseFBConfigSGIX");
	glad_glXCreateGLXPixmapWithConfigSGIX = (PFNGLXCREATEGLXPIXMAPWITHCONFIGSGIXPROC)load("glXCreateGLXPixmapWithConfigSGIX");
	glad_glXCreateContextWithConfigSGIX = (PFNGLXCREATECONTEXTWITHCONFIGSGIXPROC)load("glXCreateContextWithConfigSGIX");
	glad_glXGetVisualFromFBConfigSGIX = (PFNGLXGETVISUALFROMFBCONFIGSGIXPROC)load("glXGetVisualFromFBConfigSGIX");
	glad_glXGetFBConfigFromVisualSGIX = (PFNGLXGETFBCONFIGFROMVISUALSGIXPROC)load("glXGetFBConfigFromVisualSGIX");
}
static void load_GLX_MESA_pixmap_colormap(GLADloadproc load) {
	if(!GLAD_GLX_MESA_pixmap_colormap) return;
	glad_glXCreateGLXPixmapMESA = (PFNGLXCREATEGLXPIXMAPMESAPROC)load("glXCreateGLXPixmapMESA");
}
static void load_GLX_NV_video_output(GLADloadproc load) {
	if(!GLAD_GLX_NV_video_output) return;
	glad_glXGetVideoDeviceNV = (PFNGLXGETVIDEODEVICENVPROC)load("glXGetVideoDeviceNV");
	glad_glXReleaseVideoDeviceNV = (PFNGLXRELEASEVIDEODEVICENVPROC)load("glXReleaseVideoDeviceNV");
	glad_glXBindVideoImageNV = (PFNGLXBINDVIDEOIMAGENVPROC)load("glXBindVideoImageNV");
	glad_glXReleaseVideoImageNV = (PFNGLXRELEASEVIDEOIMAGENVPROC)load("glXReleaseVideoImageNV");
	glad_glXSendPbufferToVideoNV = (PFNGLXSENDPBUFFERTOVIDEONVPROC)load("glXSendPbufferToVideoNV");
	glad_glXGetVideoInfoNV = (PFNGLXGETVIDEOINFONVPROC)load("glXGetVideoInfoNV");
}
static void load_GLX_SGIX_dmbuffer(GLADloadproc load) {
	if(!GLAD_GLX_SGIX_dmbuffer) return;
#ifdef _DM_BUFFER_H_
	glad_glXAssociateDMPbufferSGIX = (PFNGLXASSOCIATEDMPBUFFERSGIXPROC)load("glXAssociateDMPbufferSGIX");
#else
	(void)load;
#endif
}
static void load_GLX_SGIX_swap_barrier(GLADloadproc load) {
	if(!GLAD_GLX_SGIX_swap_barrier) return;
	glad_glXBindSwapBarrierSGIX = (PFNGLXBINDSWAPBARRIERSGIXPROC)load("glXBindSwapBarrierSGIX");
	glad_glXQueryMaxSwapBarriersSGIX = (PFNGLXQUERYMAXSWAPBARRIERSSGIXPROC)load("glXQueryMaxSwapBarriersSGIX");
}
static void load_GLX_MESA_release_buffers(GLADloadproc load) {
	if(!GLAD_GLX_MESA_release_buffers) return;
	glad_glXReleaseBuffersMESA = (PFNGLXRELEASEBUFFERSMESAPROC)load("glXReleaseBuffersMESA");
}
static void load_GLX_MESA_copy_sub_buffer(GLADloadproc load) {
	if(!GLAD_GLX_MESA_copy_sub_buffer) return;
	glad_glXCopySubBufferMESA = (PFNGLXCOPYSUBBUFFERMESAPROC)load("glXCopySubBufferMESA");
}
static void load_GLX_SGI_cushion(GLADloadproc load) {
	if(!GLAD_GLX_SGI_cushion) return;
	glad_glXCushionSGI = (PFNGLXCUSHIONSGIPROC)load("glXCushionSGI");
}
static void load_GLX_NV_present_video(GLADloadproc load) {
	if(!GLAD_GLX_NV_present_video) return;
	glad_glXEnumerateVideoDevicesNV = (PFNGLXENUMERATEVIDEODEVICESNVPROC)load("glXEnumerateVideoDevicesNV");
	glad_glXBindVideoDeviceNV = (PFNGLXBINDVIDEODEVICENVPROC)load("glXBindVideoDeviceNV");
}
static void load_GLX_SUN_get_transparent_index(GLADloadproc load) {
	if(!GLAD_GLX_SUN_get_transparent_index) return;
	glad_glXGetTransparentIndexSUN = (PFNGLXGETTRANSPARENTINDEXSUNPROC)load("glXGetTransparentIndexSUN");
}
static void load_GLX_ARB_get_proc_address(GLADloadproc load) {
	if(!GLAD_GLX_ARB_get_proc_address) return;
	glad_glXGetProcAddressARB = (PFNGLXGETPROCADDRESSARBPROC)load("glXGetProcAddressARB");
}
static void find_extensionsGLX(void) {
	GLAD_GLX_ARB_framebuffer_sRGB = has_ext("GLX_ARB_framebuffer_sRGB");
	GLAD_GLX_EXT_import_context = has_ext("GLX_EXT_import_context");
	GLAD_GLX_NV_multisample_coverage = has_ext("GLX_NV_multisample_coverage");
	GLAD_GLX_SGIS_shared_multisample = has_ext("GLX_SGIS_shared_multisample");
	GLAD_GLX_SGIX_pbuffer = has_ext("GLX_SGIX_pbuffer");
	GLAD_GLX_NV_swap_group = has_ext("GLX_NV_swap_group");
	GLAD_GLX_ARB_fbconfig_float = has_ext("GLX_ARB_fbconfig_float");
	GLAD_GLX_SGIX_hyperpipe = has_ext("GLX_SGIX_hyperpipe");
	GLAD_GLX_ARB_robustness_share_group_isolation = has_ext("GLX_ARB_robustness_share_group_isolation");
	GLAD_GLX_INTEL_swap_event = has_ext("GLX_INTEL_swap_event");
	GLAD_GLX_SGIX_video_resize = has_ext("GLX_SGIX_video_resize");
	GLAD_GLX_EXT_create_context_es2_profile = has_ext("GLX_EXT_create_context_es2_profile");
	GLAD_GLX_ARB_robustness_application_isolation = has_ext("GLX_ARB_robustness_application_isolation");
	GLAD_GLX_NV_copy_image = has_ext("GLX_NV_copy_image");
	GLAD_GLX_OML_sync_control = has_ext("GLX_OML_sync_control");
	GLAD_GLX_EXT_framebuffer_sRGB = has_ext("GLX_EXT_framebuffer_sRGB");
	GLAD_GLX_SGI_make_current_read = has_ext("GLX_SGI_make_current_read");
	GLAD_GLX_MESA_swap_control = has_ext("GLX_MESA_swap_control");
	GLAD_GLX_SGI_swap_control = has_ext("GLX_SGI_swap_control");
	GLAD_GLX_EXT_fbconfig_packed_float = has_ext("GLX_EXT_fbconfig_packed_float");
	GLAD_GLX_EXT_buffer_age = has_ext("GLX_EXT_buffer_age");
	GLAD_GLX_3DFX_multisample = has_ext("GLX_3DFX_multisample");
	GLAD_GLX_EXT_visual_info = has_ext("GLX_EXT_visual_info");
	GLAD_GLX_SGI_video_sync = has_ext("GLX_SGI_video_sync");
	GLAD_GLX_MESA_agp_offset = has_ext("GLX_MESA_agp_offset");
	GLAD_GLX_SGIS_multisample = has_ext("GLX_SGIS_multisample");
	GLAD_GLX_MESA_set_3dfx_mode = has_ext("GLX_MESA_set_3dfx_mode");
	GLAD_GLX_EXT_texture_from_pixmap = has_ext("GLX_EXT_texture_from_pixmap");
	GLAD_GLX_NV_video_capture = has_ext("GLX_NV_video_capture");
	GLAD_GLX_ARB_multisample = has_ext("GLX_ARB_multisample");
	GLAD_GLX_NV_delay_before_swap = has_ext("GLX_NV_delay_before_swap");
	GLAD_GLX_SGIX_swap_group = has_ext("GLX_SGIX_swap_group");
	GLAD_GLX_EXT_swap_control = has_ext("GLX_EXT_swap_control");
	GLAD_GLX_SGIX_video_source = has_ext("GLX_SGIX_video_source");
	GLAD_GLX_MESA_query_renderer = has_ext("GLX_MESA_query_renderer");
	GLAD_GLX_ARB_create_context = has_ext("GLX_ARB_create_context");
	GLAD_GLX_EXT_create_context_es_profile = has_ext("GLX_EXT_create_context_es_profile");
	GLAD_GLX_SGIX_fbconfig = has_ext("GLX_SGIX_fbconfig");
	GLAD_GLX_MESA_pixmap_colormap = has_ext("GLX_MESA_pixmap_colormap");
	GLAD_GLX_SGIX_visual_select_group = has_ext("GLX_SGIX_visual_select_group");
	GLAD_GLX_NV_video_output = has_ext("GLX_NV_video_output");
	GLAD_GLX_SGIS_blended_overlay = has_ext("GLX_SGIS_blended_overlay");
	GLAD_GLX_SGIX_dmbuffer = has_ext("GLX_SGIX_dmbuffer");
	GLAD_GLX_ARB_create_context_robustness = has_ext("GLX_ARB_create_context_robustness");
	GLAD_GLX_SGIX_swap_barrier = has_ext("GLX_SGIX_swap_barrier");
	GLAD_GLX_EXT_swap_control_tear = has_ext("GLX_EXT_swap_control_tear");
	GLAD_GLX_MESA_release_buffers = has_ext("GLX_MESA_release_buffers");
	GLAD_GLX_EXT_visual_rating = has_ext("GLX_EXT_visual_rating");
	GLAD_GLX_MESA_copy_sub_buffer = has_ext("GLX_MESA_copy_sub_buffer");
	GLAD_GLX_SGI_cushion = has_ext("GLX_SGI_cushion");
	GLAD_GLX_NV_float_buffer = has_ext("GLX_NV_float_buffer");
	GLAD_GLX_OML_swap_method = has_ext("GLX_OML_swap_method");
	GLAD_GLX_NV_present_video = has_ext("GLX_NV_present_video");
	GLAD_GLX_SUN_get_transparent_index = has_ext("GLX_SUN_get_transparent_index");
	GLAD_GLX_AMD_gpu_association = has_ext("GLX_AMD_gpu_association");
	GLAD_GLX_ARB_create_context_profile = has_ext("GLX_ARB_create_context_profile");
	GLAD_GLX_ARB_get_proc_address = has_ext("GLX_ARB_get_proc_address");
	GLAD_GLX_ARB_vertex_buffer_object = has_ext("GLX_ARB_vertex_buffer_object");
}

static void find_coreGLX(Display *dpy, int screen) {
	int major = 0, minor = 0;
	if(dpy == 0 && GLADGLXDisplay == 0) {
		dpy = XOpenDisplay(0);
		screen = XScreenNumberOfScreen(XDefaultScreenOfDisplay(dpy));
	} else if(dpy == 0) {
		dpy = GLADGLXDisplay;
		screen = GLADGLXscreen;
	}
	glXQueryVersion(dpy, &major, &minor);
	GLADGLXDisplay = dpy;
	GLADGLXscreen = screen;
	GLAD_GLX_VERSION_1_0 = (major == 1 && minor >= 0) || major > 1;
	GLAD_GLX_VERSION_1_1 = (major == 1 && minor >= 1) || major > 1;
	GLAD_GLX_VERSION_1_2 = (major == 1 && minor >= 2) || major > 1;
	GLAD_GLX_VERSION_1_3 = (major == 1 && minor >= 3) || major > 1;
	GLAD_GLX_VERSION_1_4 = (major == 1 && minor >= 4) || major > 1;
}

void gladLoadGLXLoader(GLADloadproc load, Display *dpy, int screen) {
	glXQueryVersion = (PFNGLXQUERYVERSIONPROC)load("glXQueryVersion");
	if(glXQueryVersion == NULL) return;
	find_coreGLX(dpy, screen);
	load_GLX_VERSION_1_0(load);
	load_GLX_VERSION_1_1(load);
	load_GLX_VERSION_1_2(load);
	load_GLX_VERSION_1_3(load);
	load_GLX_VERSION_1_4(load);

	find_extensionsGLX();
	load_GLX_EXT_import_context(load);
	load_GLX_SGIX_pbuffer(load);
	load_GLX_NV_swap_group(load);
	load_GLX_SGIX_hyperpipe(load);
	load_GLX_SGIX_video_resize(load);
	load_GLX_NV_copy_image(load);
	load_GLX_OML_sync_control(load);
	load_GLX_SGI_make_current_read(load);
	load_GLX_MESA_swap_control(load);
	load_GLX_SGI_swap_control(load);
	load_GLX_SGI_video_sync(load);
	load_GLX_MESA_agp_offset(load);
	load_GLX_MESA_set_3dfx_mode(load);
	load_GLX_EXT_texture_from_pixmap(load);
	load_GLX_NV_video_capture(load);
	load_GLX_NV_delay_before_swap(load);
	load_GLX_SGIX_swap_group(load);
	load_GLX_EXT_swap_control(load);
	load_GLX_SGIX_video_source(load);
	load_GLX_MESA_query_renderer(load);
	load_GLX_ARB_create_context(load);
	load_GLX_SGIX_fbconfig(load);
	load_GLX_MESA_pixmap_colormap(load);
	load_GLX_NV_video_output(load);
	load_GLX_SGIX_dmbuffer(load);
	load_GLX_SGIX_swap_barrier(load);
	load_GLX_MESA_release_buffers(load);
	load_GLX_MESA_copy_sub_buffer(load);
	load_GLX_SGI_cushion(load);
	load_GLX_NV_present_video(load);
	load_GLX_SUN_get_transparent_index(load);
	load_GLX_ARB_get_proc_address(load);

	return;
}

