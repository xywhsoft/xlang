#include "ui/about_view.h"
#include "ui/editor_view.h"
#include "services/program_version.h"
#include "services/update_client.h"
#include "services/ui_font.h"
#include "platform/file_io.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int failed,started;
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"about_view_test:%d %s\n",__LINE__,#e); failed=1; goto cleanup; } } while(0)
static int settle(xide_app* a)
{ xideWorkbenchLayout(a);return xuiDispatchPendingEvents(a->context)==XUI_OK && xuiLayout(a->context)==XUI_OK; }
static int click(xide_app* a,xui_widget w)
{
    xui_rect_t r=xuiWidgetGetWorldRect(w); float x=r.fX+r.fW/2,y=r.fY+r.fH/2;
    return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a);
}
static int fitted(xide_app* a)
{
    xui_widget window=xideAboutViewWindow(a),scroll=xideDialogGetScrollViewWidget(window);
    xui_scroll_model_t* model=xuiScrollViewGetModel(scroll); float dx=0,dy=0;
    if(!model || xuiScrollModelGetMaxOffset(model,&dx,&dy)!=XUI_OK || dx>.01f || dy>.01f) return 0;
    xui_widget frame=xuiScrollViewGetFrameWidget(scroll);
    if(xuiWidgetGetVisible(xuiScrollFrameGetHScrollBarWidget(frame)) || xuiWidgetGetVisible(xuiScrollFrameGetVScrollBarWidget(frame))) return 0;
    xui_widget content=xideDialogGetContentWidget(window);
    xui_rect_t bounds=xuiWidgetGetWorldRect(xuiScrollViewGetViewportWidget(scroll));
    float top=INFINITY,bottom=0;int count=0;
    for(xui_widget w=xuiWidgetGetFirstChild(content);w;w=xuiWidgetGetNextSibling(w)) {
        if(xuiWidgetGetType(w)==xuiButtonGetType(a->context)) return 0;
        xui_rect_t r=xuiWidgetGetWorldRect(w);
        if((r.fW<0 || r.fH<0) || r.fX<bounds.fX-1 || r.fY<bounds.fY-1 || r.fX+r.fW>bounds.fX+bounds.fW+1 || r.fY+r.fH>bounds.fY+bounds.fH+1) return 0;
        const char* text=xuiWidgetGetType(w)==xuiLabelGetType(a->context) ? xuiLabelGetText(w) : xuiHyperlinkGetText(w);
        xui_vec2_t measured={0};
        if(xideDpiTextMeasure(&a->proxy,a->uiFont,text,&measured)!=XUI_OK || measured.fX>r.fW+1 || measured.fY>r.fH+1) return 0;
        top=fminf(top,r.fY);bottom=fmaxf(bottom,r.fY+r.fH);++count;
    }
    return count==14 && fabsf((top-bounds.fY)-(bounds.fY+bounds.fH-bottom))<=2;
}
static int verify(void* user)
{
    if(started) return XGE_OK;
    started=1; xide_app app={0}; char** argv=user; const char* root=argv[1];
    char* state=xideUpdateStateDirectoryAt(root);char* receipt=state?xidePathResolve(state,"installed.json"):NULL;
    char* settings=xidePathResolve(root,"settings.json");
    char* portable=xidePathResolve(root,"portable/data"),*bundle=portable?xidePathResolve(portable,"release.json"):NULL;
    CHECK(state && receipt && settings && xideIoCreateDirectories(state));
    CHECK(!strcmp(xideProgramVersion(),argv[2]));
    CHECK(xideInstalledIterationAt(NULL)==0 && xideInstalledIterationAt(state)==0 && !xrtGetError());
    CHECK(portable && bundle && xideIoCreateDirectories(portable));
    { char* install=xidePathResolve(root,"portable");
      const char valid[]={"{\"format\":\"xlang-release-1\",\"platform\":\"win_x64\",\"version\":1}"};
      CHECK(install && xideIoWriteAtomic(bundle,(xbytesview){(const unsigned char*)valid,sizeof(valid)-1}));
      CHECK(xideInstalledIterationAt(install)==1 && !xrtGetError());
      const char wrong[]={"{\"format\":\"xlang-release-1\",\"platform\":\"linux_x64\",\"version\":9}"};
      CHECK(xideIoWriteAtomic(bundle,(xbytesview){(const unsigned char*)wrong,sizeof(wrong)-1}));
      CHECK(xideInstalledIterationAt(install)==0 && !xrtGetError());free(install); }
    const char* invalid[]={"{broken","{\"version\":true}","{\"version\":-1}","{\"version\":2147483648}","{\"version\":1.5}"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i) {
        CHECK(xideIoWriteAtomic(receipt,(xbytesview){(const unsigned char*)invalid[i],strlen(invalid[i])}));
        CHECK(xideInstalledIterationAt(root)==0 && !xrtGetError());
    }
    const char good[]="{\"version\":42,\"displayVersion\":\"9.9.9\"}";
    CHECK(xideIoWriteAtomic(receipt,(xbytesview){(const unsigned char*)good,sizeof(good)-1}));
    CHECK(xideInstalledIterationAt(root)==42 && !strcmp(xideProgramVersion(),argv[2]));
    xrtSetErrorKind(XERR_IO); const xerror* previous=xrtGetError();CHECK(previous);
    CHECK(xideInstalledIterationAt(root)==42 && xrtGetError()==previous);xrtClearError();
    app.hidden=1;app.noSession=1;app.disableAutomaticDiagnostics=1;app.viewportWidth=1280;app.viewportHeight=800;
    app.settingsPath=settings;
    CHECK(xideAppCreate(&app)==XUI_OK && settle(&app));
    const xide_action_info* action=xideActionNamed("help.about");CHECK(action && action->id==XIDE_ACTION_ABOUT);
    CHECK(xideActionExecute(&app,XIDE_ACTION_ABOUT)==XIDE_ACTION_DISPATCHED && settle(&app));
    CHECK(xideAboutViewIsOpen(&app) && xideActionsHasModal(&app));
    CHECK(xuiWidgetGetType(xideAboutViewWindow(&app))==xuiWindowGetType(app.context));
    CHECK(fitted(&app));
    CHECK(!xideActionQuery(&app,XIDE_ACTION_NEW_FILE).enabled);
    CHECK(click(&app,xuiWindowGetCloseButtonWidget(xideAboutViewWindow(&app))) && !xideActionsHasModal(&app));
    CHECK(xideActionExecute(&app,XIDE_ACTION_ABOUT)==XIDE_ACTION_DISPATCHED && settle(&app));
    CHECK(xuiGetFocusWidget(app.context)==xideAboutViewLinkWidget(&app,0));
    CHECK(xuiInputKeyDown(app.context,XUI_KEY_ESCAPE,0)==XUI_OK && xuiInputKeyUp(app.context,XUI_KEY_ESCAPE,0)==XUI_OK && settle(&app));
    CHECK(!xideAboutViewIsOpen(&app) && xideActionQuery(&app,XIDE_ACTION_NEW_FILE).enabled);
    CHECK(xideActionExecute(&app,XIDE_ACTION_ABOUT)==XIDE_ACTION_DISPATCHED && settle(&app));
    CHECK(click(&app,xuiWindowGetCloseButtonWidget(xideAboutViewWindow(&app))) && !xideAboutViewIsOpen(&app));
    CHECK(xideActionExecute(&app,XIDE_ACTION_ABOUT)==XIDE_ACTION_DISPATCHED && settle(&app));
    const int sizes[]={16,18,20,24};
    for(size_t i=0;i<sizeof(sizes)/sizeof(*sizes);++i) {
        char* font=strdup(app.uiFontPath);xide_model_error error={0};
        int applied=xideUiFontApply(&app,font,sizes[i],&error);free(font);CHECK(applied && settle(&app) && fitted(&app));
    }
    { char* font=strdup(app.uiFontPath);xide_model_error error={0};int applied=xideUiFontApply(&app,font,18,&error);free(font);CHECK(applied); }
    app.viewportWidth=640;app.viewportHeight=400;CHECK(settle(&app) && fitted(&app));
    CHECK(xuiWindowSetMaximized(xideAboutViewWindow(&app),1)==XUI_OK && settle(&app) && fitted(&app));
    CHECK(xuiWindowSetMaximized(xideAboutViewWindow(&app),0)==XUI_OK && settle(&app) && fitted(&app));
    app.viewportWidth=640;app.viewportHeight=480;CHECK(settle(&app));
    xui_rect_t r=xuiWidgetGetRect(xideAboutViewWindow(&app));CHECK(r.fW<=640 && r.fH<=480);
    xideUpdateApplyCheckResult(&app,4,0);CHECK(xideUpdateIsRequired(&app));
    CHECK(click(&app,xuiWindowGetCloseButtonWidget(xideAboutViewWindow(&app))) && !xideAboutViewIsOpen(&app));
    CHECK(xideUpdateIsRequired(&app) && xideActionsHasModal(&app) && !xideActionQuery(&app,XIDE_ACTION_ABOUT).enabled);
    xideUpdateApplyCheckResult(&app,0,0);CHECK(!xideActionsHasModal(&app));
    puts("PASS compact centered about layout: no body buttons, zero scroll range, 16/18/20/24 fonts, 640x400 viewport, maximize/restore, Escape/X, modal cleanup and required-update gate");
cleanup:
    xideAppDestroy(&app);xgeSetQuitRequestCallback(NULL,NULL);free(state);free(receipt);free(settings);free(portable);free(bundle);xgeQuit();return XGE_OK;
}
int main(int argc,char** argv)
{
    if(argc!=3) return 2;
    xge_desc_t d={0};d.iWidth=1280;d.iHeight=800;d.sTitle="xlang about window test";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&d);if(result==XGE_OK) result=xgeRun(verify,argv);xgeUnit();return result==XGE_OK && started && !failed ? 0 : 1;
}
