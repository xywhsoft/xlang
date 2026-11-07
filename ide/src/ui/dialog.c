#include "app/xide_app.h"
#include "ui/dialog.h"
#include "services/ui_font.h"
#include "theme/theme.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct dialog_row {
    struct dialog_row* next; xui_widget row,line,label,spacer,children[12];
    float height; size_t count; int field,block,grow,footer;
    float measured[12],measureScale;uint64_t textHash[12];xui_font measureFont;
} dialog_row;
struct xide_dialog {
    xide_app* app; xide_dialog* next;
    xui_widget window,scroll,content,backdrop,restore;
    xui_widget_action_proc cancel; void* cancelUser;
    xui_widget_event_proc event; void* eventUser;
    xui_popup_change_proc change; void* changeUser;
    float width,height,padding,maxWidth,maxHeight,lastWidth,lastHeight;
    int open,placed,resized,maximized;
    xui_widget footer,divider; dialog_row *rows,*tail;
    float minWidth,minHeight,bodyWidth,bodyHeight;
    int form,fixedBody;
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
static void cancel(xui_widget w,void* user);
static int windowEvent(xui_widget w,const xui_event_t* event,void* user) {
    xide_dialog* d=user;
    int skip=event && event->iType==XUI_EVENT_POINTER_DOWN && event->iPhase==XUI_EVENT_PHASE_CAPTURE && event->pTarget!=w;
    int r=!skip && d->event ? d->event(w,event,d->eventUser) : XUI_OK;
    /* Child controls and IME get the first chance to consume Escape. Keep
       modal cancellation explicit when the window bubbles an unhandled key. */
    if(d->open && r==XUI_OK && event && event->iType==XUI_EVENT_KEY_DOWN &&
       event->iKey==XUI_KEY_ESCAPE && event->iPhase!=XUI_EVENT_PHASE_CAPTURE) {
        cancel(w,d);return XUI_EVENT_DISPATCH_STOP;
    }
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
#include "ui/dialog_form.inc"
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
    /* The overlay root must preserve the explicit viewport-sized rectangle. */
    xuiWidgetSetFlowMode(d->backdrop,XUI_FLOW_ABSOLUTE);
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
    xuiWidgetSetUserData(w,NULL); xuiWidgetDestroy(d->backdrop); xuiWidgetDestroy(w);
    while(d->rows) { dialog_row* next=d->rows->next;free(d->rows);d->rows=next; } free(d);
}
void xideDialogDestroyAll(xide_app* a) { while(a->dialogs) xideDialogDestroy(a->dialogs->window); }
static void layout(xide_dialog* d) {
    xide_app* a=d->app; float scale=xideUiFontScale(a),pad=d->padding*scale,title=32*scale;
    xui_rect_t rect=xuiWidgetGetRect(d->window);
    int maximized=xuiWindowIsMaximized(d->window);
    if(!maximized && !d->maximized && d->placed && d->lastWidth && (fabsf(rect.fW-d->lastWidth)>.5f || fabsf(rect.fH-d->lastHeight)>.5f)) d->resized=1;
    if(!maximized && !d->resized) { rect.fW=d->width*(d->form ? scale:1)+2*pad+2; rect.fH=d->height*(d->form ? scale:1)+2*pad+title+2; }
    float mw=d->maxWidth>0 ? d->maxWidth : a->viewportWidth-32, mh=d->maxHeight>0 ? d->maxHeight : a->viewportHeight-32;
    if(mw<120) mw=120;
    if(mh<100) mh=100;
    if(rect.fW>mw) rect.fW=mw;
    if(rect.fH>mh) rect.fH=mh;
    float minW=fminf((d->minWidth>0 ? d->minWidth:240)*scale,mw),minH=fminf((d->minHeight>0 ? d->minHeight:160)*scale,mh);
    if(rect.fW<minW) rect.fW=minW;
    if(rect.fH<minH) rect.fH=minH;
    if(!d->placed) { rect.fX=(a->viewportWidth-rect.fW)/2; rect.fY=(a->viewportHeight-rect.fH)/2; }
    if(rect.fX<0) rect.fX=0;
    if(rect.fY<0) rect.fY=0;
    if(rect.fX+rect.fW>a->viewportWidth) rect.fX=fmaxf(0,a->viewportWidth-rect.fW);
    if(rect.fY+rect.fH>a->viewportHeight) rect.fY=fmaxf(0,a->viewportHeight-rect.fH);
    if(maximized) rect=(xui_rect_t){0,0,a->viewportWidth,a->viewportHeight};
    d->maximized=maximized;
    xuiWindowSetChrome(d->window,title,1,6*scale,24*scale);
    xuiWindowSetMinSize(d->window,minW,minH);
    xuiWidgetSetRect(d->window,rect); d->lastWidth=rect.fW; d->lastHeight=rect.fH;
    if(d->form) { int result=layoutForm(d,rect.fW-2,rect.fH-title-2,scale);if(result!=XUI_OK)d->app->frameError=result; }
    else xuiWidgetSetRect(d->scroll,(xui_rect_t){pad,pad,fmaxf(1,rect.fW-2-2*pad),fmaxf(1,rect.fH-title-2-2*pad)});
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
        xuiOverlayAttach(d->app->context,NULL,w,XUI_LAYER_FLOATING,DIALOG_WINDOW_Z);
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
int xideDialogSetContentSize(xui_widget w,float width,float height) { xide_dialog* d=dialog(w); if(!d) return XUI_ERROR_INVALID_ARGUMENT; if(d->form)return XUI_OK;d->width=width;d->height=height; return xuiScrollViewSetContentSize(d->scroll,width,height); }
int xideDialogGetContentSize(xui_widget w,float* width,float* height) { xide_dialog* d=dialog(w); return d ? xuiScrollViewGetContentSize(d->scroll,width,height) : XUI_ERROR_INVALID_ARGUMENT; }
int xideDialogSetMaxSize(xui_widget w,float width,float height) { xide_dialog* d=dialog(w); if(!d) return XUI_ERROR_INVALID_ARGUMENT; d->maxWidth=width;d->maxHeight=height; return XUI_OK; }
int xideDialogSetFocusRestore(xui_widget w,xui_widget focus) { xide_dialog* d=dialog(w);if(!d) return XUI_ERROR_INVALID_ARGUMENT; d->restore=focus; return XUI_OK; }
int xideDialogSetScroll(xui_widget w,float x,float y) { xide_dialog* d=dialog(w); return d ? xuiScrollViewSetOffset(d->scroll,x,y) : XUI_ERROR_INVALID_ARGUMENT; }
int xideDialogGetScroll(xui_widget w,float* x,float* y) { xide_dialog* d=dialog(w); return d ? xuiScrollViewGetOffset(d->scroll,x,y) : XUI_ERROR_INVALID_ARGUMENT; }
int xideDialogSetChange(xui_widget w,xui_popup_change_proc cb,void* user) { xide_dialog* d=dialog(w);if(!d) return XUI_ERROR_INVALID_ARGUMENT; d->change=cb;d->changeUser=user;return XUI_OK; }
int xideDialogSetCancel(xui_widget w,xui_widget_action_proc cb,void* user) { xide_dialog* d=dialog(w);if(!d) return XUI_ERROR_INVALID_ARGUMENT; d->cancel=cb;d->cancelUser=user;return XUI_OK; }

static int messageContentLayout(xui_widget content,xui_rect_t area,void* user) {
    xui_msgbox box=user;xui_context context=xuiWidgetGetContext(content);
    xui_msgbox_metrics_t metrics={0};metrics.iSize=sizeof(metrics);xuiMsgBoxGetMetrics(box,&metrics);
    for(xui_widget c=xuiWidgetGetFirstChild(content);c;c=xuiWidgetGetNextSibling(c)) {
        xui_rect_t rect=xuiWidgetGetRect(c);
        if(xuiWidgetGetType(c)==xuiSeparatorGetType(context))rect=(xui_rect_t){area.fX,fmaxf(area.fY,area.fY+area.fH-metrics.fFooterHeight),area.fW,1};
        else if(xuiWidgetGetType(c)==xuiTextEditGetType(context))rect=xuiMsgBoxGetMessageRect(box);
        int result=xuiLayoutArrangeChild(content,c,rect);if(result!=XUI_OK)return result;
    }
    return XUI_OK;
}
/* Native messages autosize from their text and already reserve a footer.
 * Disable resize chrome that the native autosizing contract cannot honor. */
int xideDialogStyleMessage(xide_app* a,xui_msgbox box) {
    if(!a || !box)return XUI_ERROR_INVALID_ARGUMENT;
    xui_widget window=xuiMsgBoxGetWindowWidget(box),content=xuiMsgBoxGetContentWidget(box),line=NULL;
    for(xui_widget c=xuiWidgetGetFirstChild(content);c;c=xuiWidgetGetNextSibling(c))
        if(xuiWidgetGetType(c)==xuiSeparatorGetType(a->context)) {line=c;break;}
    if(!line) {
        xui_separator_desc_t desc={0};desc.iSize=sizeof(desc);desc.fThickness=1;desc.iOrientation=XUI_ORIENTATION_HORIZONTAL;
        int r=xuiSeparatorCreate(a->context,&line,&desc);if(r!=XUI_OK)return r;
        r=xuiWidgetAddChild(content,line);if(r!=XUI_OK){xuiWidgetDestroy(line);return r;}
        xuiWidgetSetFlowMode(line,XUI_FLOW_ABSOLUTE);
    }
    xui_msgbox_metrics_t metrics={0};metrics.iSize=sizeof(metrics);xuiMsgBoxGetMetrics(box,&metrics);
    xui_rect_t rect=xuiWidgetGetRect(content);
    xuiSeparatorSetColor(line,ideThemeColor(a->theme,IDE_BORDER));
    xuiWidgetSetRect(line,(xui_rect_t){0,fmaxf(0,rect.fH-metrics.fFooterHeight),rect.fW,1});
    const char* text=xuiMsgBoxGetMessage(box);size_t size=text ? strlen(text):0;int paragraphs=0;
    for(size_t i=0;i<size;++i)if(text[i]=='\n')++paragraphs;
    if(size>1024 || paragraphs>12) {
        xui_widget details=NULL;
        for(xui_widget c=xuiWidgetGetFirstChild(content);c;c=xuiWidgetGetNextSibling(c))
            if(xuiWidgetGetType(c)==xuiTextEditGetType(a->context)){details=c;break;}
        if(!details) {
            xui_text_edit_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=a->uiFont;desc.sText=text;desc.bReadonly=1;desc.bWordWrap=1;
            int result=xuiTextEditCreate(a->context,&details,&desc);if(result!=XUI_OK)return result;
            result=xuiWidgetAddChild(content,details);if(result!=XUI_OK){xuiWidgetDestroy(details);return result;}
            xuiWidgetSetAccessibleName(details,"消息详细内容（可选择和滚动）");
        }
    }
    xuiWidgetSetLayoutChildrenCallback(content,messageContentLayout,box);
    xuiWindowSetResizeEdges(window,0);return xuiWindowSetShowMaximize(window,0);
}
