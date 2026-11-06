#include "ui/about_view.h"
#include "services/program_version.h"
#include "services/ui_font.h"
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#define TRY(call) do { int r_=(call); if(r_!=XUI_OK) return r_; } while(0)
typedef struct about_key { xide_app* app; xui_widget_event_proc original; void* user; } about_key;
struct xide_about_view { xui_widget window,content,heading,description,names[6],values[6]; float scale; about_key keys[8]; int keyCount; };
static const char* urls[]={"https://xcompiler.com/","https://xywhsoft.com/","https://xcompiler.com/tour.html","https://xcompiler.com/book/index.html"};
const char* xideAboutViewLink(int index) { return index>=0 && index<4 ? urls[index] : NULL; }
static void closeWindow(xui_widget w,void* user)
{
    xide_app* a=user; (void)w; xideDialogSetOpen(a->aboutView->window,0); xideActionsRefresh(a); xgeRenderRequest();
}
static int keyDown(xui_widget w,const xui_event_t* e,void* user)
{
    about_key* key=user;
    if(e->iKey==XUI_KEY_ESCAPE) { closeWindow(w,key->app); return XUI_EVENT_DISPATCH_STOP; }
    return key->original ? key->original(w,e,key->user) : XUI_OK;
}
static int bindKey(xide_app* a,xui_widget w)
{
    if(!w) return XUI_OK;
    struct xide_about_view* v=a->aboutView;
    if(v->keyCount>=8) return XUI_ERROR_LIMIT_EXCEEDED;
    about_key* key=&v->keys[v->keyCount++]; key->app=a;
    TRY(xuiWidgetGetEventHandler(w,XUI_EVENT_KEY_DOWN,&key->original,&key->user));
    return xuiWidgetSetEventHandler(w,XUI_EVENT_KEY_DOWN,keyDown,key);
}
static void openLink(xui_widget w,void* user)
{
    xide_app* a=user; int index=(int)(intptr_t)xuiWidgetGetUserData(w);
    const char* url=xideAboutViewLink(index); if(!url) return;
    /* Fixed HTTPS destinations only; no command shell or downloaded URL. */
    wchar_t wide[128]; MultiByteToWideChar(CP_UTF8,0,url,-1,wide,128);
    if((INT_PTR)ShellExecuteW(NULL,L"open",wide,NULL,NULL,SW_SHOWNORMAL)<=32)
        xideMessageShow(a,"打开网站","无法打开浏览器，请检查系统的默认浏览器设置。",1);
}
static int label(xide_app* a,const char* text,float x,float y,float width,float height,xui_widget* out)
{
    xui_label_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=text; d.iWrapMode=XUI_TEXT_WRAP_WORD;
    xui_widget w=NULL; TRY(xuiLabelCreate(a->context,&w,&d)); TRY(xuiWidgetSetRect(w,(xui_rect_t){x,y,width,height}));
    TRY(xuiWidgetAddChild(a->aboutView->content,w)); if(out) *out=w; return XUI_OK;
}
/* Arrange inside the actual viewport, rather than a second scaled virtual
   canvas. Fractional font/DPI scales must never create scrollable overflow. */
static int layoutContent(xui_widget content,xui_rect_t area,void* user)
{
    xide_app* a=user; struct xide_about_view* v=a->aboutView;
    float scale=xideUiFontScale(a),line=20*scale;
    xui_font_metrics_t metrics={0};
    if(a->proxy.fontGetMetrics(&a->proxy,a->uiFont,&metrics)==XUI_OK && metrics.fLineHeight>0) line=ceilf(metrics.fLineHeight);
    float names=0,values=0;
    for(int i=0;i<6;++i) {
        xui_vec2_t size={0};
        TRY(xideDpiTextMeasure(&a->proxy,a->uiFont,xuiLabelGetText(v->names[i]),&size)); names=fmaxf(names,ceilf(size.fX));
        const char* text=i<2 ? xuiLabelGetText(v->values[i]) : xuiHyperlinkGetText(v->values[i]);
        TRY(xideDpiTextMeasure(&a->proxy,a->uiFont,text,&size)); values=fmaxf(values,ceilf(size.fX));
    }
    float gap=20*scale,group=fminf(area.fW,names+gap+values),x=area.fX+(area.fW-group)/2;
    float compact=fminf(1,fmaxf(0,(area.fH-9*line)/(60*scale)));
    float row=line+6*scale*compact,heading=line+4*scale*compact,intro=2*line+4*scale*compact;
    float height=heading+intro+16*scale*compact+6*row,y=area.fY+fmaxf(0,(area.fH-height)/2);
    TRY(xuiLayoutArrangeChild(content,v->heading,(xui_rect_t){area.fX,y,area.fW,heading})); y+=heading+6*scale*compact;
    TRY(xuiLayoutArrangeChild(content,v->description,(xui_rect_t){area.fX,y,area.fW,intro})); y+=intro+10*scale*compact;
    for(int i=0;i<6;++i) {
        TRY(xuiLayoutArrangeChild(content,v->names[i],(xui_rect_t){x,y,names,row}));
        TRY(xuiLayoutArrangeChild(content,v->values[i],(xui_rect_t){x+names+gap,y,fmaxf(1,group-names-gap),row})); y+=row;
    }
    return XUI_OK;
}
static int create(xide_app* a)
{
    struct xide_about_view* v=calloc(1,sizeof(*v)); if(!v) return XUI_ERROR_OUT_OF_MEMORY; a->aboutView=v;
    TRY(xideDialogCreate(a,&v->window,"关于 xlang",420,250,18));
    TRY(xideDialogSetCancel(v->window,closeWindow,a)); v->content=xideDialogGetContentWidget(v->window);
    TRY(xuiWidgetAddChild(a->root,v->window)); TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
    TRY(label(a,"xlang",0,0,420,26,&v->heading));
    TRY(label(a,"面向 C 与 xlang 的轻量集成开发环境。\n代码编辑 · 语义分析 · 项目管理 · 编译运行",0,32,420,44,&v->description));
    TRY(xuiLabelSetTextFlags(v->heading,XUI_TEXT_ALIGN_CENTER|XUI_TEXT_ALIGN_MIDDLE));
    TRY(xuiLabelSetTextFlags(v->description,XUI_TEXT_ALIGN_CENTER|XUI_TEXT_ALIGN_MIDDLE));
    TRY(label(a,"版本",0,88,112,26,&v->names[0])); TRY(label(a,"",132,88,288,26,&v->values[0]));
    TRY(label(a,"作者",0,114,112,26,&v->names[1])); TRY(label(a,"xLeaves (xywhsoft)",132,114,288,26,&v->values[1]));
    const char* names[]={"官方网站","工作室网站","xlang 语法导览","xlang 在线教程"};
    const char* texts[]={"xcompiler.com  ↗","xywhsoft.com  ↗","xcompiler.com/tour.html  ↗","xcompiler.com/book/index.html  ↗"};
    for(int i=0;i<4;++i) {
        float y=140+26*i; TRY(label(a,names[i],0,y,112,26,&v->names[i+2]));
        xui_hyperlink_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=texts[i];
        xui_widget w=NULL; TRY(xuiHyperlinkCreate(a->context,&w,&d)); TRY(xuiWidgetSetRect(w,(xui_rect_t){132,y,288,26})); v->values[i+2]=w;
        xuiWidgetSetUserData(w,(void*)(intptr_t)i); TRY(xuiHyperlinkSetClick(w,openLink,a));
        TRY(xuiWidgetSetAccessibleName(w,urls[i])); TRY(xuiWidgetAddChild(v->content,w));
        TRY(bindKey(a,w));
    }
    for(int i=0;i<6;++i) {
        TRY(xuiLabelSetTextFlags(v->names[i],XUI_TEXT_ALIGN_LEFT|XUI_TEXT_ALIGN_MIDDLE));
        if(i<2) TRY(xuiLabelSetTextFlags(v->values[i],XUI_TEXT_ALIGN_LEFT|XUI_TEXT_ALIGN_MIDDLE));
        else TRY(xuiHyperlinkSetTextFlags(v->values[i],XUI_TEXT_ALIGN_LEFT|XUI_TEXT_ALIGN_MIDDLE));
    }
    TRY(bindKey(a,v->window));
    TRY(bindKey(a,xuiWindowGetCloseButtonWidget(v->window))); TRY(bindKey(a,xuiWindowGetMaximizeButtonWidget(v->window)));
    return xuiWidgetSetLayoutChildrenCallback(v->content,layoutContent,a);
}
int xideAboutViewOpen(xide_app* a)
{
    if(!a || xideActionsHasModal(a)) return 0;
    if(!a->aboutView && create(a)!=XUI_OK) {
        if(a->aboutView) { if(a->aboutView->window) xideDialogDestroy(a->aboutView->window); xideAboutViewDestroy(a); }
        xideMessageShow(a,"关于 xlang","无法创建关于窗口。",1); return 0;
    }
    char text[128]; int64_t iteration=xideInstalledIteration();
    snprintf(text,sizeof(text),"%s  ·  更新迭代 %lld",xideProgramVersion(),(long long)iteration);
    xuiLabelSetText(a->aboutView->values[0],text); xideAboutViewLayout(a);
    xideUiFontBindWidget(a,a->aboutView->window); xideDialogSetOpen(a->aboutView->window,1);
    xuiSetFocusWidget(a->context,a->aboutView->values[2]); xgeRenderRequest(); return 1;
}
void xideAboutViewLayout(xide_app* a)
{
    struct xide_about_view* v=a->aboutView; if(!v) return;
    xideDialogSetMaxSize(v->window,a->viewportWidth-32,a->viewportHeight-32);
    float scale=xideUiFontScale(a);
    if(v->scale!=scale) {
        xideDialogSetContentSize(v->window,xideUiHeight(a,420),xideUiHeight(a,250));
        /* Zero extent uses the native scroll view's viewport-filling content.
           The layout above fits that space, so both maximum offsets stay zero. */
        xuiScrollViewSetContentSize(xideDialogGetScrollViewWidget(v->window),0,0); v->scale=scale;
    }
}
int xideAboutViewIsOpen(xide_app* a) { return a && a->aboutView && xideDialogIsOpen(a->aboutView->window); }
xui_widget xideAboutViewWindow(xide_app* a) { return a && a->aboutView ? a->aboutView->window : NULL; }
xui_widget xideAboutViewLinkWidget(xide_app* a,int i) { return a && a->aboutView && i>=0 && i<4 ? a->aboutView->values[i+2] : NULL; }
void xideAboutViewDestroy(xide_app* a) { free(a->aboutView); a->aboutView=NULL; }
