#ifndef XLANG_IDE_WINDOW_STATE_H
#define XLANG_IDE_WINDOW_STATE_H
#include "platform/state_file.h"
typedef struct xide_window_geometry {
    int x,y,width,height,workX,workY,dpi,maximized;
    char monitor[128];
} xide_window_geometry;
typedef struct xide_window_rect { int x,y,width,height; } xide_window_rect;
typedef struct xide_window_state {
    xide_state_file file;
    void *window,*previousProc;
    void (*requestFrame)(void);
    uint64_t fingerprint,due;
    int ready,blocked;
} xide_window_state;
int xideWindowDpi(void* window);
int xideWindowCapture(void* window,xide_window_geometry* geometry,xide_model_error* error);
int xideWindowRestore(void* window,const xide_window_geometry* geometry,xide_model_error* error);
int xideWindowPlan(const xide_window_geometry* geometry,xide_window_rect work,int dpi,int minWidth,int minHeight,xide_window_rect* rectangle);
int xideWindowGeometryRead(const xvalue* value,xide_window_geometry* geometry);
int xideWindowStateOpen(xide_window_state* state,void* window,const char* directory,int restore,void (*requestFrame)(void),xide_model_error* error);
int xideWindowStateSave(xide_window_state* state,xide_model_error* error);
xvalue* xideWindowStateCaptureRoot(const xide_window_state* state,void* window,xide_model_error* error);
int xideWindowStatePoll(xide_window_state* state,xide_model_error* error);
void xideWindowStateClose(xide_window_state* state);
#endif
