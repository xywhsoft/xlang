/* A real bounded local backend, not an XGE wrapper. Precompile when a target's
 * system EGL SDK is not part of TinyCC's bundled headers. The resulting helper
 * depends only on OS libraries, leaving one program-owned XRT implementation. */
#include "foreign_xge3_host_probe.h"
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#include <GL/gl.h>
#if defined(XL6_TEST_HOST_PROBE_DLL)
#define PROBE_API __declspec(dllexport)
#else
#define PROBE_API
#endif
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <pthread.h>
#define PROBE_API __attribute__((visibility("default")))
#endif
static struct {
    xge_test_host_probe_info info;
    int backend;
    bool deferRelease,deferred;
#if defined(_WIN32)
    DWORD creator;
    HWND window;
    HDC dc;
    HGLRC context,temporary;
    bool classOwned;
#else
    pthread_t creator;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    bool initialized;
#endif
} probe;
static bool creator(void)
{
#if defined(_WIN32)
    return probe.creator==GetCurrentThreadId();
#else
    return pthread_equal(probe.creator,pthread_self())!=0;
#endif
}
PROBE_API int xgeTestHostConfigure(int backend,bool defer)
{
    if(probe.info.contextLive || probe.info.platformLive)return XGE_ERROR_BUSY;
#if defined(_WIN32)
    if(probe.classOwned || probe.temporary)return XGE_ERROR_BUSY;
#endif
    if(backend!=XGE_GPU_BACKEND_OPENGL33 && backend!=XGE_GPU_BACKEND_GLES30)return XGE_ERROR_UNSUPPORTED;
#if defined(_WIN32)
    if(backend!=XGE_GPU_BACKEND_OPENGL33)return XGE_ERROR_UNSUPPORTED;
#endif
    memset(&probe,0,sizeof(probe));probe.backend=backend;probe.deferRelease=defer;
#if defined(_WIN32)
    probe.creator=GetCurrentThreadId();
#else
    probe.creator=pthread_self();
#endif
    return XGE_OK;
}
PROBE_API int xgeTestHostPlatformInit(void)
{
    if(!creator() || probe.info.platformInit++)return XGE_ERROR_INVALID_STATE;
#if defined(_WIN32)
    WNDCLASSW cls={0};cls.style=CS_OWNDC;cls.lpfnWndProc=DefWindowProcW;
    cls.hInstance=GetModuleHandleW(NULL);cls.lpszClassName=L"xlang-owned-host-probe";
    probe.classOwned=RegisterClassW(&cls)!=0;
    if(!probe.classOwned && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return XGE_ERROR_BACKEND_FAILED;
    probe.info.platformLive=probe.classOwned;
    probe.window=CreateWindowW(cls.lpszClassName,L"Local host test",WS_OVERLAPPEDWINDOW,
        0,0,64,64,NULL,NULL,cls.hInstance,NULL);
    probe.info.platformLive=probe.info.platformLive || probe.window!=NULL;
    if(!probe.window)return XGE_ERROR_BACKEND_FAILED;
    probe.dc=GetDC(probe.window);if(!probe.dc)return XGE_ERROR_BACKEND_FAILED;
    PIXELFORMATDESCRIPTOR p={0};p.nSize=sizeof(p);p.nVersion=1;
    p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=32;p.cAlphaBits=8;
    int format=ChoosePixelFormat(probe.dc,&p);
    if(!format || !SetPixelFormat(probe.dc,format,&p))return XGE_ERROR_BACKEND_FAILED;
#else
    PFNEGLGETPLATFORMDISPLAYEXTPROC get=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if(!get)return XGE_ERROR_UNSUPPORTED;
    probe.display=get(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL);
    if(probe.display==EGL_NO_DISPLAY || !eglInitialize(probe.display,NULL,NULL))return XGE_ERROR_BACKEND_FAILED;
    probe.initialized=true;probe.info.platformLive=true;
#endif
    return XGE_OK;
}
PROBE_API int xgeTestHostGraphicsInit(void)
{
    if(!creator() || !probe.info.platformLive || probe.info.graphicsInit++)return XGE_ERROR_INVALID_STATE;
#if defined(_WIN32)
    probe.temporary=wglCreateContext(probe.dc);
    if(!probe.temporary || !wglMakeCurrent(probe.dc,probe.temporary))return XGE_ERROR_BACKEND_FAILED;
    typedef HGLRC (WINAPI *Create)(HDC,HGLRC,const int*);
    Create create=(Create)wglGetProcAddress("wglCreateContextAttribsARB");
    const int attrs[]={0x2091,3,0x2092,3,0x9126,1,0};
    if(create)probe.context=create(probe.dc,NULL,attrs);
    bool unbound=wglMakeCurrent(NULL,NULL)!=0,deleted=false;
    if(unbound)deleted=wglDeleteContext(probe.temporary)!=0;
    if(!unbound || !deleted)return XGE_ERROR_BACKEND_FAILED;
    probe.temporary=NULL;
    if(!probe.context)return XGE_ERROR_UNSUPPORTED;
#else
    bool es=probe.backend==XGE_GPU_BACKEND_GLES30;
    if(!eglBindAPI(es ? EGL_OPENGL_ES_API:EGL_OPENGL_API))return XGE_ERROR_BACKEND_FAILED;
    EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,
        es ? EGL_OPENGL_ES3_BIT:EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,
        EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
    EGLConfig config=NULL;EGLint n=0;
    if(!eglChooseConfig(probe.display,attrs,&config,1,&n) || n!=1)return XGE_ERROR_BACKEND_FAILED;
    EGLint surface[]={EGL_WIDTH,4,EGL_HEIGHT,4,EGL_NONE};
    probe.surface=eglCreatePbufferSurface(probe.display,config,surface);
    const EGLint gl[]={EGL_CONTEXT_MAJOR_VERSION,3,EGL_CONTEXT_MINOR_VERSION,3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,EGL_NONE};
    const EGLint gles[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    probe.context=eglCreateContext(probe.display,config,EGL_NO_CONTEXT,es ? gles:gl);
    if(probe.surface==EGL_NO_SURFACE || probe.context==EGL_NO_CONTEXT)return XGE_ERROR_BACKEND_FAILED;
#endif
    probe.info.contextLive=true;return XGE_OK;
}
PROBE_API int xgeTestHostMakeCurrent(void)
{
    if(!creator() || !probe.info.contextLive)return XGE_ERROR_INVALID_STATE;
    ++probe.info.makeCurrent;
#if defined(_WIN32)
    return wglMakeCurrent(probe.dc,probe.context) ? XGE_OK:XGE_ERROR_BACKEND_FAILED;
#else
    return eglMakeCurrent(probe.display,probe.surface,probe.surface,probe.context) ? XGE_OK:XGE_ERROR_BACKEND_FAILED;
#endif
}
PROBE_API int xgeTestHostIsCurrent(void)
{
    if(!creator())return XGE_ERROR_BUSY;
#if defined(_WIN32)
    return probe.context && wglGetCurrentContext()==probe.context;
#else
    return probe.context!=EGL_NO_CONTEXT && eglGetCurrentContext()==probe.context;
#endif
}
PROBE_API xge_host_release_result_t xgeTestHostRelease(void)
{
    if(!creator())return (xge_host_release_result_t){XGE_ERROR_BUSY,false};
    ++probe.info.releases;
    if(probe.deferRelease && !probe.deferred){probe.deferred=true;return (xge_host_release_result_t){XGE_OK,false};}
#if defined(_WIN32)
    if(probe.context){
        if(wglGetCurrentContext()==probe.context && !wglMakeCurrent(NULL,NULL))return (xge_host_release_result_t){XGE_ERROR_BACKEND_FAILED,false};
        if(!wglDeleteContext(probe.context))return (xge_host_release_result_t){XGE_ERROR_BACKEND_FAILED,false};
        probe.context=NULL;
    }
    if(probe.temporary){
        if(wglGetCurrentContext()==probe.temporary && !wglMakeCurrent(NULL,NULL))return (xge_host_release_result_t){XGE_ERROR_BACKEND_FAILED,false};
        if(!wglDeleteContext(probe.temporary))return (xge_host_release_result_t){XGE_ERROR_BACKEND_FAILED,false};
        probe.temporary=NULL;
    }
    if(probe.dc){if(!ReleaseDC(probe.window,probe.dc))return (xge_host_release_result_t){XGE_ERROR_BACKEND_FAILED,false};probe.dc=NULL;}
    if(probe.window){if(!DestroyWindow(probe.window))return (xge_host_release_result_t){XGE_ERROR_BACKEND_FAILED,false};probe.window=NULL;}
    if(probe.classOwned){
        if(!UnregisterClassW(L"xlang-owned-host-probe",GetModuleHandleW(NULL)))return (xge_host_release_result_t){XGE_ERROR_BACKEND_FAILED,false};
        probe.classOwned=false;
    }
#else
    if(probe.context!=EGL_NO_CONTEXT){
        if(eglGetCurrentContext()==probe.context && !eglMakeCurrent(probe.display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT))return (xge_host_release_result_t){XGE_ERROR_BACKEND_FAILED,false};
        if(!eglDestroyContext(probe.display,probe.context))return (xge_host_release_result_t){XGE_ERROR_BACKEND_FAILED,false};
        probe.context=EGL_NO_CONTEXT;
    }
    if(probe.surface!=EGL_NO_SURFACE){if(!eglDestroySurface(probe.display,probe.surface))return (xge_host_release_result_t){XGE_ERROR_BACKEND_FAILED,false};probe.surface=EGL_NO_SURFACE;}
    if(probe.initialized){if(!eglTerminate(probe.display))return (xge_host_release_result_t){XGE_ERROR_BACKEND_FAILED,false};probe.initialized=false;}
    probe.display=EGL_NO_DISPLAY;
#endif
    probe.info.contextLive=probe.info.platformLive=false;
    return (xge_host_release_result_t){XGE_OK,true};
}
PROBE_API void xgeTestHostPlatformUnit(void){++probe.info.platformUnit;}
PROBE_API void xgeTestHostGraphicsUnit(void){++probe.info.graphicsUnit;}
PROBE_API int xgeTestHostBegin(void){++probe.info.begins;return XGE_OK;}
PROBE_API int xgeTestHostEnd(void){++probe.info.ends;return XGE_OK;}
PROBE_API int xgeTestHostPoll(void){++probe.info.polls;return 9;}
PROBE_API void xgeTestHostPresent(void){++probe.info.presents;}
PROBE_API int xgeTestHostInfo(xge_test_host_probe_info* out)
{if(!out)return XGE_ERROR_INVALID_ARGUMENT;*out=probe.info;return XGE_OK;}
