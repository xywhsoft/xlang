#include "ui/about_view.h"
#include "services/program_version.h"
#include "services/ui_font.h"
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#define TRY(call) do { int r_=(call); if(r_!=XUI_OK) return r_; } while(0)
typedef struct about_key { xide_app* app; xui_widget_event_proc original; void* user; } about_key;
struct xide_about_view { xui_widget window,content,version,close; xide_ui_layout layout; float scale; about_key keys[8]; int keyCount; };
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
static int create(xide_app* a)
{
    struct xide_about_view* v=calloc(1,sizeof(*v)); if(!v) return XUI_ERROR_OUT_OF_MEMORY; a->aboutView=v;
    TRY(xideDialogCreate(a,&v->window,"关于 xlang",580,388,22));
    TRY(xideDialogSetCancel(v->window,closeWindow,a)); v->content=xideDialogGetContentWidget(v->window);
    TRY(xuiWidgetAddChild(a->root,v->window)); TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
    TRY(label(a,"xlang",0,0,580,36,NULL));
    TRY(label(a,"轻量、高效的集成开发环境，为 C 与 xlang 提供代码编辑、语义分析、项目管理与编译运行。",0,46,580,48,NULL));
    TRY(label(a,"版本",0,112,112,28,NULL)); TRY(label(a,"",122,112,458,28,&v->version));
    TRY(label(a,"作者",0,150,112,28,NULL)); TRY(label(a,"xLeaves (xywhsoft)",122,150,458,28,NULL));
    const char* names[]={"官方网站","工作室网站","xlang 语法导览","xlang 在线教程"};
    const char* texts[]={"xcompiler.com  ↗","xywhsoft.com  ↗","xcompiler.com/tour.html  ↗","xcompiler.com/book/index.html  ↗"};
    for(int i=0;i<4;++i) {
        float y=188+36*i; TRY(label(a,names[i],0,y,118,28,NULL));
        xui_hyperlink_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=texts[i];
        xui_widget w=NULL; TRY(xuiHyperlinkCreate(a->context,&w,&d)); TRY(xuiWidgetSetRect(w,(xui_rect_t){122,y,458,28}));
        xuiWidgetSetUserData(w,(void*)(intptr_t)i); TRY(xuiHyperlinkSetClick(w,openLink,a));
        TRY(xuiWidgetSetAccessibleName(w,urls[i])); TRY(xuiWidgetAddChild(v->content,w));
        TRY(bindKey(a,w));
    }
    xui_button_desc_t b={0}; b.iSize=sizeof(b); b.pFont=a->uiFont; b.sText="关闭窗口";
    TRY(xuiButtonCreate(a->context,&v->close,&b)); TRY(xuiWidgetSetRect(v->close,(xui_rect_t){464,348,116,32}));
    TRY(xuiButtonSetClick(v->close,closeWindow,a)); TRY(xuiWidgetAddChild(v->content,v->close));
    TRY(bindKey(a,v->close)); TRY(bindKey(a,v->window));
    TRY(bindKey(a,xuiWindowGetCloseButtonWidget(v->window))); TRY(bindKey(a,xuiWindowGetMaximizeButtonWidget(v->window)));
    return xideUiLayoutCapture(&v->layout,v->content);
}
int xideAboutViewOpen(xide_app* a)
{
    if(!a || xideActionsHasModal(a)) return 0;
    if(!a->aboutView && create(a)!=XUI_OK) {
        if(a->aboutView) { if(a->aboutView->window) xideDialogDestroy(a->aboutView->window); xideAboutViewDestroy(a); }
        xideMessageShow(a,"关于 xlang","无法创建关于窗口。",1); return 0;
    }
    char text[128]; int64_t iteration=xideInstalledIteration();
    if(iteration) snprintf(text,sizeof(text),"%s  ·  更新迭代 %lld",xideProgramVersion(),(long long)iteration);
    else snprintf(text,sizeof(text),"%s  ·  更新迭代 0（尚未安装更新）",xideProgramVersion());
    xuiLabelSetText(a->aboutView->version,text); xideAboutViewLayout(a);
    xideUiFontBindWidget(a,a->aboutView->window); xideDialogSetOpen(a->aboutView->window,1);
    xuiSetFocusWidget(a->context,a->aboutView->close); xgeRenderRequest(); return 1;
}
void xideAboutViewLayout(xide_app* a)
{
    struct xide_about_view* v=a->aboutView; if(!v) return;
    xideDialogSetMaxSize(v->window,a->viewportWidth-32,a->viewportHeight-32);
    float scale=xideUiFontScale(a);
    if(v->scale!=scale) { xideUiLayoutApply(a,&v->layout,1); xideDialogSetContentSize(v->window,xideUiHeight(a,580),xideUiHeight(a,388)); v->scale=scale; }
}
int xideAboutViewIsOpen(xide_app* a) { return a && a->aboutView && xideDialogIsOpen(a->aboutView->window); }
xui_widget xideAboutViewWindow(xide_app* a) { return a && a->aboutView ? a->aboutView->window : NULL; }
xui_widget xideAboutViewCloseButton(xide_app* a) { return a && a->aboutView ? a->aboutView->close : NULL; }
void xideAboutViewDestroy(xide_app* a) { free(a->aboutView); a->aboutView=NULL; }
