#define _WIN32_WINNT 0x0601
#include "platform/window_state.h"
#include "platform/app_resources.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define WINDOW_PROPERTY L"XlangIDE.WindowState"
#define PROCEDURE_PROPERTY L"XlangIDE.WindowPreviousProc"
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* root,const char* key) { return root ? xrtValueObjectGet(root,sv(key)) : NULL; }
static int put(xvalue* root,const char* key,xvalue* value) { return value && xrtValueObjectSetNew(root,sv(key),value); }
static int fail(xide_model_error* error,int code,const char* message) { if(error) { error->code=code; snprintf(error->message,sizeof(error->message),"%s",message); } return 0; }
static int number(const xvalue* root,const char* key,int low,int high,int* result)
{
    int64_t value=0;
    if(!xrtValueGetInt(get(root,key),&value) || value<low || value>high) return 0;
    *result=(int)value; return 1;
}
int xideWindowGeometryRead(const xvalue* root,xide_window_geometry* geometry)
{
    xide_window_geometry fresh={0}; xstrview monitor={0},coordinates={0}; bool maximized=false;
    if(!root || !geometry || !xrtValueIs(root,XVALUE_OBJECT) || !xrtValueGetString(get(root,"coordinateSystem"),&coordinates) || coordinates.Size!=sizeof("monitor-work-dip-96")-1 || memcmp(coordinates.Data,"monitor-work-dip-96",coordinates.Size) ||
       !number(root,"x",-1000000,1000000,&fresh.x) || !number(root,"y",-1000000,1000000,&fresh.y) ||
       !number(root,"width",64,32768,&fresh.width) || !number(root,"height",64,32768,&fresh.height) ||
       !number(root,"workX",-1000000,1000000,&fresh.workX) || !number(root,"workY",-1000000,1000000,&fresh.workY) || !number(root,"dpi",48,768,&fresh.dpi) ||
       !xrtValueGetBool(get(root,"maximized"),&maximized) || !xrtValueGetString(get(root,"monitor"),&monitor) || monitor.Size>=sizeof(fresh.monitor) || memchr(monitor.Data,0,monitor.Size)) return 0;
    memcpy(fresh.monitor,monitor.Data,monitor.Size); fresh.maximized=maximized; *geometry=fresh; return 1;
}
int xideWindowDpi(void* handle)
{
    HWND window=(HWND)handle;
    typedef UINT (WINAPI *get_dpi)(HWND);
    get_dpi getDpi=(get_dpi)(uintptr_t)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");
    UINT dpi=getDpi ? getDpi(window) : 0;
    if(dpi>=48 && dpi<=768) return (int)dpi;
    HDC dc=GetDC(window); int fallback=dc ? GetDeviceCaps(dc,LOGPIXELSX) : 96; if(dc) ReleaseDC(window,dc);
    return fallback>=48 && fallback<=768 ? fallback : 96;
}
int xideWindowSetApplicationIcon(void* handle)
{
    HWND window=(HWND)handle;
    if(!window) return 0;
    typedef int (WINAPI *get_metric)(int,UINT);
    get_metric metric=(get_metric)(uintptr_t)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetSystemMetricsForDpi");
    UINT dpi=(UINT)xideWindowDpi(window);
    const int metrics[]={SM_CXICON,SM_CYICON,SM_CXSMICON,SM_CYSMICON}; int sizes[4];
    for(int i=0;i<4;++i) sizes[i]=metric ? metric(metrics[i],dpi) : GetSystemMetrics(metrics[i]);
    HINSTANCE module=GetModuleHandleW(NULL);
    /* Shared resource handles remain valid for the process lifetime. */
    HICON large=(HICON)LoadImageW(module,MAKEINTRESOURCEW(XIDE_RESOURCE_APP_ICON),IMAGE_ICON,sizes[0],sizes[1],LR_SHARED);
    HICON small=(HICON)LoadImageW(module,MAKEINTRESOURCEW(XIDE_RESOURCE_APP_ICON),IMAGE_ICON,sizes[2],sizes[3],LR_SHARED);
    if(!large || !small) return 0;
    SendMessageW(window,WM_SETICON,ICON_BIG,(LPARAM)large);
    SendMessageW(window,WM_SETICON,ICON_SMALL,(LPARAM)small);
    return 1;
}
static void minimum(HWND window,int dpi,int* width,int* height)
{
    typedef BOOL (WINAPI *adjust_dpi)(LPRECT,DWORD,BOOL,DWORD,UINT);
    adjust_dpi adjust=(adjust_dpi)(uintptr_t)GetProcAddress(GetModuleHandleW(L"user32.dll"),"AdjustWindowRectExForDpi");
    RECT rect={0,0,MulDiv(640,dpi,96),MulDiv(480,dpi,96)};
    DWORD style=(DWORD)GetWindowLongPtrW(window,GWL_STYLE),extra=(DWORD)GetWindowLongPtrW(window,GWL_EXSTYLE);
    if(adjust) adjust(&rect,style,FALSE,extra,(UINT)dpi); else AdjustWindowRectEx(&rect,style,FALSE,extra);
    *width=rect.right-rect.left; *height=rect.bottom-rect.top;
}
int xideWindowPlan(const xide_window_geometry* geometry,xide_window_rect work,int dpi,int minWidth,int minHeight,xide_window_rect* rectangle)
{
    if(!geometry || !rectangle || dpi<48 || dpi>768 || work.width<=0 || work.height<=0 || geometry->width<64 || geometry->width>32768 || geometry->height<64 || geometry->height>32768 || geometry->x<-1000000 || geometry->x>1000000 || geometry->y<-1000000 || geometry->y>1000000 || minWidth<1 || minHeight<1) return 0;
    xide_window_rect rect={work.x+MulDiv(geometry->x,dpi,96),work.y+MulDiv(geometry->y,dpi,96),MulDiv(geometry->width,dpi,96),MulDiv(geometry->height,dpi,96)};
    if(rect.width<minWidth) rect.width=minWidth;
    if(rect.height<minHeight) rect.height=minHeight;
    if(rect.width>work.width) rect.width=work.width;
    if(rect.height>work.height) rect.height=work.height;
    if(rect.x<work.x) rect.x=work.x;
    if(rect.y<work.y) rect.y=work.y;
    if(rect.x>work.x+work.width-rect.width) rect.x=work.x+work.width-rect.width;
    if(rect.y>work.y+work.height-rect.height) rect.y=work.y+work.height-rect.height;
    *rectangle=rect; return 1;
}
int xideWindowCapture(void* native,xide_window_geometry* geometry,xide_model_error* error)
{
    HWND window=native; if(error) memset(error,0,sizeof(*error));
    if(!window || !geometry || !IsWindow(window) || ((DWORD)GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)) return fail(error,XIDE_MODEL_INVALID,"No native main window to capture.");
    WINDOWPLACEMENT placement={0}; placement.length=sizeof(placement);
    if(!GetWindowPlacement(window,&placement)) return fail(error,XIDE_MODEL_IO,"Cannot read native window placement.");
    HMONITOR monitor=MonitorFromRect(&placement.rcNormalPosition,MONITOR_DEFAULTTONEAREST); MONITORINFOEXW info={0}; info.cbSize=sizeof(info);
    if(!monitor || !GetMonitorInfoW(monitor,(MONITORINFO*)&info)) return fail(error,XIDE_MODEL_IO,"Cannot read the window's monitor.");
    xide_window_geometry fresh={0}; fresh.dpi=xideWindowDpi(window);
    int tool=((DWORD)GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)!=0;
    int originX=tool ? info.rcWork.left : info.rcMonitor.left,originY=tool ? info.rcWork.top : info.rcMonitor.top;
    fresh.x=MulDiv(placement.rcNormalPosition.left-originX,96,fresh.dpi); fresh.y=MulDiv(placement.rcNormalPosition.top-originY,96,fresh.dpi);
    fresh.width=MulDiv(placement.rcNormalPosition.right-placement.rcNormalPosition.left,96,fresh.dpi); fresh.height=MulDiv(placement.rcNormalPosition.bottom-placement.rcNormalPosition.top,96,fresh.dpi);
    fresh.workX=info.rcWork.left; fresh.workY=info.rcWork.top;
    fresh.maximized=IsZoomed(window) || (IsIconic(window) && (placement.flags&WPF_RESTORETOMAXIMIZED));
    if(fresh.width<64 || fresh.height<64 || fresh.width>32768 || fresh.height>32768) return fail(error,XIDE_MODEL_INVALID,"Native normal window dimensions are outside the supported range.");
    if(!WideCharToMultiByte(CP_UTF8,0,info.szDevice,-1,fresh.monitor,sizeof(fresh.monitor),NULL,NULL)) return fail(error,XIDE_MODEL_INVALID,"Cannot encode monitor identity.");
    *geometry=fresh; return 1;
}
typedef struct monitor_search { const char* device; HMONITOR result; } monitor_search;
static BOOL CALLBACK findMonitor(HMONITOR monitor,HDC dc,LPRECT rectangle,LPARAM user)
{
    (void)dc; (void)rectangle; monitor_search* search=(monitor_search*)user; MONITORINFOEXW info={0}; info.cbSize=sizeof(info); char device[128]={0};
    if(GetMonitorInfoW(monitor,(MONITORINFO*)&info) && WideCharToMultiByte(CP_UTF8,0,info.szDevice,-1,device,sizeof(device),NULL,NULL) && !strcmp(device,search->device)) { search->result=monitor; return FALSE; }
    return TRUE;
}
int xideWindowRestore(void* native,const xide_window_geometry* geometry,xide_model_error* error)
{
    HWND window=native; if(error) memset(error,0,sizeof(*error));
    if(!window || !geometry || !IsWindow(window) || ((DWORD)GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)) return fail(error,XIDE_MODEL_INVALID,"No native main window to restore.");
    monitor_search search={geometry->monitor,NULL}; EnumDisplayMonitors(NULL,NULL,findMonitor,(LPARAM)&search);
    POINT point={geometry->workX,geometry->workY}; HMONITOR monitor=search.result ? search.result : MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST);
    MONITORINFO info={0}; info.cbSize=sizeof(info); if(!monitor || !GetMonitorInfoW(monitor,&info)) return fail(error,XIDE_MODEL_IO,"Cannot locate a usable monitor.");
    /* A hidden probe inherits this thread's awareness and reports target DPI,
       without using GetDpiForMonitor on a per-monitor-aware thread. */
    HWND probe=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"",WS_POPUP,info.rcWork.left+1,info.rcWork.top+1,1,1,NULL,NULL,GetModuleHandleW(NULL),NULL);
    int dpi=xideWindowDpi(probe ? probe : window); if(probe) DestroyWindow(probe);
    int minWidth=0,minHeight=0; minimum(window,dpi,&minWidth,&minHeight); xide_window_rect rect={0};
    if(!xideWindowPlan(geometry,(xide_window_rect){info.rcWork.left,info.rcWork.top,info.rcWork.right-info.rcWork.left,info.rcWork.bottom-info.rcWork.top},dpi,minWidth,minHeight,&rect)) return fail(error,XIDE_MODEL_INVALID,"Invalid saved window rectangle.");
    WINDOWPLACEMENT placement={0}; placement.length=sizeof(placement);
    if(!GetWindowPlacement(window,&placement)) return fail(error,XIDE_MODEL_IO,"Cannot read current window placement.");
    int tool=((DWORD)GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)!=0;
    int offsetX=tool ? 0 : info.rcWork.left-info.rcMonitor.left,offsetY=tool ? 0 : info.rcWork.top-info.rcMonitor.top;
    placement.rcNormalPosition=(RECT){rect.x-offsetX,rect.y-offsetY,rect.x+rect.width-offsetX,rect.y+rect.height-offsetY};
    placement.flags=0; placement.ptMinPosition=(POINT){-1,-1}; placement.ptMaxPosition=(POINT){-1,-1};
    placement.showCmd=IsWindowVisible(window) ? geometry->maximized ? SW_SHOWMAXIMIZED : SW_SHOWNOACTIVATE : SW_HIDE;
    if(!SetWindowPlacement(window,&placement)) return fail(error,XIDE_MODEL_IO,"Cannot restore native window placement.");
    return 1;
}
static LRESULT CALLBACK windowProcedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam)
{
    xide_window_state* state=(xide_window_state*)GetPropW(window,WINDOW_PROPERTY);
    WNDPROC previous=(WNDPROC)GetPropW(window,PROCEDURE_PROPERTY);
    LRESULT result=previous ? CallWindowProcW(previous,window,message,wParam,lParam) : DefWindowProcW(window,message,wParam,lParam);
    if(state && message==WM_GETMINMAXINFO && lParam) {
        MINMAXINFO* info=(MINMAXINFO*)lParam; int width=0,height=0; minimum(window,xideWindowDpi(window),&width,&height);
        if(info->ptMinTrackSize.x<width) info->ptMinTrackSize.x=width;
        if(info->ptMinTrackSize.y<height) info->ptMinTrackSize.y=height;
    }
    if(state && state->requestFrame && (message==WM_MOVE || message==WM_SIZE || message==WM_WINDOWPOSCHANGED || message==WM_DPICHANGED)) state->requestFrame();
    if(message==WM_NCDESTROY) { if(state) state->window=NULL; RemovePropW(window,WINDOW_PROPERTY); RemovePropW(window,PROCEDURE_PROPERTY); }
    return result;
}
static uint64_t fingerprint(const xide_window_geometry* geometry)
{
    uint64_t code=UINT64_C(1469598103934665603); const unsigned char* bytes=(const unsigned char*)geometry;
    for(size_t i=0;i<sizeof(*geometry);++i) { code^=bytes[i]; code*=UINT64_C(1099511628211); } return code;
}
void xideWindowStateClose(xide_window_state* state)
{
    if(!state) return;
    HWND window=state->window;
    if(window && IsWindow(window)) {
        if((WNDPROC)GetWindowLongPtrW(window,GWLP_WNDPROC)==windowProcedure) { SetWindowLongPtrW(window,GWLP_WNDPROC,(LONG_PTR)state->previousProc); RemovePropW(window,PROCEDURE_PROPERTY); }
        if((WNDPROC)GetWindowLongPtrW(window,GWLP_WNDPROC)==(WNDPROC)state->previousProc) RemovePropW(window,PROCEDURE_PROPERTY);
        RemovePropW(window,WINDOW_PROPERTY);
    }
    xideStateClear(&state->file); memset(state,0,sizeof(*state));
}
int xideWindowStateOpen(xide_window_state* state,void* native,const char* directory,int restore,void (*requestFrame)(void),xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!state || !directory || !native || !IsWindow(native)) return fail(error,XIDE_MODEL_INVALID,"Invalid native window-state arguments.");
    xideWindowStateClose(state);
    if(GetPropW((HWND)native,WINDOW_PROPERTY)) return fail(error,XIDE_MODEL_INVALID,"Window state is already attached to this window.");
    state->window=native; state->requestFrame=requestFrame;
    HWND window=native;
    /* A later native subclass (for example the SDK's TSF hook) may still
       forward through our detached procedure. Reuse that link instead of
       inserting ourselves above it and forming a cycle. Close retains its
       forwarding property until our procedure is again at the chain head. */
    WNDPROC previous=(WNDPROC)GetPropW(window,PROCEDURE_PROPERTY);
    int retained=previous!=NULL;
    if(!previous) previous=(WNDPROC)GetWindowLongPtrW(window,GWLP_WNDPROC);
    state->previousProc=(void*)previous;
    if(!previous || !SetPropW(window,WINDOW_PROPERTY,state) || !SetPropW(window,PROCEDURE_PROPERTY,(HANDLE)previous)) { xideWindowStateClose(state); return fail(error,XIDE_MODEL_IO,"Cannot attach native window-state handling."); }
    if(!retained) {
        SetLastError(0); LONG_PTR old=SetWindowLongPtrW(window,GWLP_WNDPROC,(LONG_PTR)windowProcedure);
        if(!old && GetLastError()) { xideWindowStateClose(state); return fail(error,XIDE_MODEL_IO,"Cannot attach the native window procedure."); }
    }
    char* path=xidePathResolve(directory,"window.json"); state->ready=path && xideStateOpen(&state->file,path,"xlang-ide-window-1",error); free(path);
    xide_window_geometry saved={0};
    if(state->ready && state->file.existed && !xideWindowGeometryRead(get(state->file.source,"placement"),&saved)) { state->ready=0; return fail(error,XIDE_MODEL_INVALID,"Window state is invalid; the original file is preserved."); }
    if(!state->ready) return 0;
    if(restore && state->file.existed && !xideWindowRestore(native,&saved,error)) { state->ready=0; return 0; }
    xide_window_geometry current={0}; if(!xideWindowCapture(native,&current,error)) { state->ready=0; return 0; }
    state->fingerprint=fingerprint(&current); return 1;
}
xvalue* xideWindowStateCaptureRoot(const xide_window_state* state,void* window,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    xide_window_geometry current={0}; if(!xideWindowCapture(window,&current,error)) return NULL;
    xvalue *root=state && state->ready ? xrtValueDeepClone(state->file.source) : xrtValueObject(),*placement=get(root,"placement") ? xrtValueDeepClone(get(root,"placement")) : xrtValueObject();
    const char* keys[]={"x","y","width","height","workX","workY","dpi"}; int values[]={current.x,current.y,current.width,current.height,current.workX,current.workY,current.dpi};
    int okay=root && placement && put(root,"format",xrtValueString(sv("xlang-ide-window-1")));
    for(int i=0;i<7 && okay;++i) okay=put(placement,keys[i],xrtValueInt(values[i]));
    if(okay) okay=put(placement,"coordinateSystem",xrtValueString(sv("monitor-work-dip-96"))) && put(placement,"monitor",xrtValueString(sv(current.monitor))) && put(placement,"maximized",xrtValueBool(current.maximized));
    if(okay) { xvalue* owned=placement; placement=NULL; okay=put(root,"placement",owned); }
    xrtValueRelease(placement);
    if(!okay) { xrtValueRelease(root); if(error && !error->message[0]) fail(error,XIDE_MODEL_MEMORY,"Cannot construct native window state."); return NULL; }
    return root;
}
int xideWindowStateSave(xide_window_state* state,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!state || !state->window || !state->ready) return 1;
    xvalue* root=xideWindowStateCaptureRoot(state,state->window,error);
    int okay=root && xideStateWrite(&state->file,root,error); xrtValueRelease(root); if(!okay) return 0;
    xide_window_geometry current={0}; if(!xideWindowCapture(state->window,&current,error)) return 0;
    state->fingerprint=fingerprint(&current); state->due=0; state->blocked=0; return 1;
}
int xideWindowStatePoll(xide_window_state* state,xide_model_error* error)
{
    if(!state || !state->window || !state->ready || state->blocked) return 1;
    xide_window_geometry current={0}; if(!xideWindowCapture(state->window,&current,error)) return 0;
    uint64_t code=fingerprint(&current),now=GetTickCount64(); if(code!=state->fingerprint) { state->fingerprint=code; state->due=now+750; }
    if(state->due && now>=state->due && !xideWindowStateSave(state,error)) { state->blocked=1; return 0; }
    if(state->due && state->requestFrame) state->requestFrame();
    return 1;
}
