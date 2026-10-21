#include "app/xide_app.h"
#include "ui/dialog.h"
#include "services/ui_font.h"
#include <stdlib.h>
#include <math.h>
struct xide_dialog {
    xide_app* app; xide_dialog* next;
    xui_widget window,scroll,content,backdrop,restore;
    xui_widget_action_proc cancel; void* cancelUser;
    xui_widget_event_proc event; void* eventUser;
    xui_popup_change_proc change; void* changeUser;
    float width,height,padding,maxWidth,maxHeight,lastWidth,lastHeight;
    int open,placed,resized,maximized;
};
static xide_dialog* dialog(xui_widget w) { return w ? xuiWidgetGetUserData(w) : NULL; }
/* Business windows cover the workbench, while their SDK menus remain in the
   higher POPUP layer and message boxes remain in the higher MODAL layer. */
enum { DIALOG_BACKDROP_Z=2000,DIALOG_WINDOW_Z=2001 };
static void layers(xui_widget w) {
    xuiWidgetSetLayer(w,XUI_LAYER_FLOATING,DIALOG_WINDOW_Z);
    for(xui_widget c=xuiWidgetGetFirstChild(w);c;c=xuiWidgetGetNextSibling(c)) layers(c);
}
static int consume(xui_widget w,const xui_event_t* event,void* user) { return XUI_EVENT_DISPATCH_STOP; }
static int windowEvent(xui_widget w,const xui_event_t* event,void* user) {
    xide_dialog* d=user;
    int skip=event && event->iType==XUI_EVENT_POINTER_DOWN && event->iPhase==XUI_EVENT_PHASE_CAPTURE && event->pTarget!=w;
    int r=!skip && d->event ? d->event(w,event,d->eventUser) : XUI_OK;
    if(d->open) layers(w);
    return r;
}
static void cancel(xui_widget w,void* user) {
    xide_dialog* d=user;
    /* XUI closes before notifying. Keep the operation's own cancel guard in
       charge (resource rebinding and recovery may still be settling). */
    xuiWindowSetOpen(d->window,1);
    if(d->cancel) d->cancel(w,d->cancelUser); else xideDialogSetOpen(d->window,0);
}
int xideDialogCreate(xide_app* a,xui_widget* out,const char* title,float width,float height,float padding) {
    if(!a || !out) return XUI_ERROR_INVALID_ARGUMENT;
    *out=NULL; xide_dialog* d=calloc(1,sizeof(*d)); if(!d) return XUI_ERROR_OUT_OF_MEMORY;
    d->app=a; d->width=width; d->height=height; d->padding=padding;
    xui_window_desc_t desc={0}; desc.iSize=sizeof(desc); desc.sTitle=title; desc.pFont=a->uiFont;
    desc.bClosed=1; desc.bHideCollapse=1;
    desc.fTitleBarHeight=32; desc.fBorderWidth=1; desc.fResizeGrip=6; desc.fButtonSize=24; desc.fMinWidth=240; desc.fMinHeight=160;
    int r=xuiWindowCreate(a->context,&d->window,&desc);
    if(r==XUI_OK) {
        xuiWidgetSetUserData(d->window,d);
        xui_scroll_view_desc_t scroll={0}; scroll.iSize=sizeof(scroll); scroll.fContentWidth=width; scroll.fContentHeight=height;
        r=xuiScrollViewCreate(a->context,&d->scroll,&scroll);
    }
    if(r==XUI_OK) {
        xui_widget client=xuiWindowGetClientWidget(d->window);
        xuiWidgetSetLayoutType(client,XUI_LAYOUT_MANUAL);xuiWidgetSetPadding(client,(xui_thickness_t){0});
        d->content=xuiScrollViewGetContentWidget(d->scroll); r=xuiWidgetAddChild(client,d->scroll);
    }
    if(r==XUI_OK) { xui_panel_desc_t panel={0}; panel.iSize=sizeof(panel); r=xuiPanelCreate(a->context,&d->backdrop,&panel); }
    if(r!=XUI_OK) { if(d->window) xuiWidgetDestroy(d->window); if(d->backdrop) xuiWidgetDestroy(d->backdrop); free(d); return r; }
    xuiWidgetSetVisible(d->backdrop,0); xuiWidgetSetEnabled(d->backdrop,0);
    xui_style_property_t style[3]={0};
    const char* names[]={"panel.background.color","panel.client.color","panel.border.color"};
    for(int i=0;i<3;++i) { style[i].iSize=sizeof(style[i]);style[i].sName=names[i];style[i].tValue.iSize=sizeof(style[i].tValue);style[i].tValue.iType=XUI_STYLE_VALUE_TOKEN;style[i].tValue.sText=i==0 ? "ide.shadow" : "ide.transparent"; }
    xuiWidgetSetInlineStyle(d->backdrop,style,3);
    const int events[]={XUI_EVENT_POINTER_DOWN,XUI_EVENT_POINTER_UP,XUI_EVENT_POINTER_MOVE,XUI_EVENT_KEY_DOWN,XUI_EVENT_POINTER_WHEEL};
    for(size_t i=0;i<sizeof(events)/sizeof(*events);++i) xuiWidgetSetEventHandler(d->backdrop,events[i],consume,d);
    xuiWindowSetClose(d->window,cancel,d); xuiWidgetSetCancelAction(d->window,cancel,d);
    xuiWidgetGetEventCallback(d->window,&d->event,&d->eventUser);
    xuiWidgetSetEventCallback(d->window,windowEvent,d);
    d->next=a->dialogs; a->dialogs=d; *out=d->window; return XUI_OK;
}
void xideDialogDestroy(xui_widget w) {
    xide_dialog* d=dialog(w); if(!d) return;
    xide_dialog** p=&d->app->dialogs; while(*p && *p!=d) p=&(*p)->next; if(*p) *p=d->next;
    xuiWidgetSetUserData(w,NULL); xuiWidgetDestroy(d->backdrop); xuiWidgetDestroy(w); free(d);
}
void xideDialogDestroyAll(xide_app* a) { while(a->dialogs) xideDialogDestroy(a->dialogs->window); }
static void layout(xide_dialog* d) {
    xide_app* a=d->app; float scale=xideUiFontScale(a),pad=d->padding*scale,title=32*scale;
    xui_rect_t rect=xuiWidgetGetRect(d->window);
    int maximized=xuiWindowIsMaximized(d->window);
    if(!maximized && !d->maximized && d->placed && d->lastWidth && (fabsf(rect.fW-d->lastWidth)>.5f || fabsf(rect.fH-d->lastHeight)>.5f)) d->resized=1;
    if(!maximized && !d->resized) { rect.fW=d->width+2*pad+2; rect.fH=d->height+2*pad+title+2; }
    float mw=d->maxWidth>0 ? d->maxWidth : a->viewportWidth-32, mh=d->maxHeight>0 ? d->maxHeight : a->viewportHeight-32;
    if(mw<120) mw=120;
    if(mh<100) mh=100;
    if(rect.fW>mw) rect.fW=mw;
    if(rect.fH>mh) rect.fH=mh;
    if(!d->placed) { rect.fX=(a->viewportWidth-rect.fW)/2; rect.fY=(a->viewportHeight-rect.fH)/2; }
    if(rect.fX<0) rect.fX=0;
    if(rect.fY<0) rect.fY=0;
    if(rect.fX+rect.fW>a->viewportWidth) rect.fX=fmaxf(0,a->viewportWidth-rect.fW);
    if(rect.fY+rect.fH>a->viewportHeight) rect.fY=fmaxf(0,a->viewportHeight-rect.fH);
    if(maximized) rect=(xui_rect_t){0,0,a->viewportWidth,a->viewportHeight};
    d->maximized=maximized;
    xuiWindowSetChrome(d->window,title,1,6*scale,24*scale);
    xuiWindowSetMinSize(d->window,fminf(240*scale,mw),fminf(160*scale,mh));
    xuiWidgetSetRect(d->window,rect); d->lastWidth=rect.fW; d->lastHeight=rect.fH;
    xuiWidgetSetRect(d->scroll,(xui_rect_t){pad,pad,fmaxf(1,rect.fW-2-2*pad),fmaxf(1,rect.fH-title-2-2*pad)});
    xuiWidgetSetRect(d->backdrop,(xui_rect_t){0,0,a->viewportWidth,a->viewportHeight});
    layers(d->window);
}
void xideDialogLayoutAll(xide_app* a) { for(xide_dialog* d=a->dialogs;d;d=d->next) if(d->open) layout(d); }
int xideDialogSetOpen(xui_widget w,int open) {
    xide_dialog* d=dialog(w); if(!d) return XUI_ERROR_INVALID_ARGUMENT;
    open=!!open; if(open==d->open) { int r=xuiWindowSetOpen(w,open);if(open) layers(w);return r; }
    if(open) {
        if(!d->restore) d->restore=xuiGetFocusWidget(d->app->context);
        layout(d); d->placed=1;
        xuiWidgetSetVisible(d->backdrop,1); xuiWidgetSetEnabled(d->backdrop,1);
        xuiOverlayAttach(d->app->context,w,d->backdrop,XUI_LAYER_FLOATING,DIALOG_BACKDROP_Z);
        xuiWindowSetOpen(w,1); xuiWindowBringToFront(w); layers(w);xuiOverlayBringToFront(w);
    } else {
        xuiWindowSetOpen(w,0); xuiWidgetSetVisible(d->backdrop,0); xuiWidgetSetEnabled(d->backdrop,0);
        if(xuiWidgetGetParent(d->backdrop)) xuiOverlayDetach(d->backdrop);
        if(d->restore) xuiSetFocusWidget(d->app->context,d->restore);
        d->restore=NULL;
    }
    d->open=open; if(d->change) d->change(w,open,d->changeUser); xgeRenderRequest(); return XUI_OK;
}
int xideDialogIsOpen(xui_widget w) { xide_dialog* d=dialog(w); return d && d->open; }
xui_widget xideDialogGetContentWidget(xui_widget w) { xide_dialog* d=dialog(w); return d ? d->content : NULL; }
xui_widget xideDialogGetScrollViewWidget(xui_widget w) { xide_dialog* d=dialog(w); return d ? d->scroll : NULL; }
int xideDialogSetContentSize(xui_widget w,float width,float height) { xide_dialog* d=dialog(w); if(!d) return XUI_ERROR_INVALID_ARGUMENT; d->width=width;d->height=height; return xuiScrollViewSetContentSize(d->scroll,width,height); }
int xideDialogGetContentSize(xui_widget w,float* width,float* height) { xide_dialog* d=dialog(w); return d ? xuiScrollViewGetContentSize(d->scroll,width,height) : XUI_ERROR_INVALID_ARGUMENT; }
int xideDialogSetMaxSize(xui_widget w,float width,float height) { xide_dialog* d=dialog(w); if(!d) return XUI_ERROR_INVALID_ARGUMENT; d->maxWidth=width;d->maxHeight=height; return XUI_OK; }
int xideDialogSetFocusRestore(xui_widget w,xui_widget focus) { xide_dialog* d=dialog(w);if(!d) return XUI_ERROR_INVALID_ARGUMENT; d->restore=focus; return XUI_OK; }
int xideDialogSetScroll(xui_widget w,float x,float y) { xide_dialog* d=dialog(w); return d ? xuiScrollViewSetOffset(d->scroll,x,y) : XUI_ERROR_INVALID_ARGUMENT; }
int xideDialogGetScroll(xui_widget w,float* x,float* y) { xide_dialog* d=dialog(w); return d ? xuiScrollViewGetOffset(d->scroll,x,y) : XUI_ERROR_INVALID_ARGUMENT; }
int xideDialogSetChange(xui_widget w,xui_popup_change_proc cb,void* user) { xide_dialog* d=dialog(w);if(!d) return XUI_ERROR_INVALID_ARGUMENT; d->change=cb;d->changeUser=user;return XUI_OK; }
int xideDialogSetCancel(xui_widget w,xui_widget_action_proc cb,void* user) { xide_dialog* d=dialog(w);if(!d) return XUI_ERROR_INVALID_ARGUMENT; d->cancel=cb;d->cancelUser=user;return XUI_OK; }
