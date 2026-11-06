#include "ui/about_view.h"
#include "ui/editor_view.h"
#include "services/program_version.h"
#include "services/update_client.h"
#include "platform/file_io.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failed,started;
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"about_view_test:%d %s\n",__LINE__,#e); failed=1; goto cleanup; } } while(0)
static int settle(xide_app* a)
{ xideWorkbenchLayout(a);return xuiDispatchPendingEvents(a->context)==XUI_OK && xuiLayout(a->context)==XUI_OK; }
static int click(xide_app* a,xui_widget w)
{
    xui_rect_t r=xuiWidgetGetWorldRect(w); float x=r.fX+r.fW/2,y=r.fY+r.fH/2;
    return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a);
}
static int verify(void* user)
{
    if(started) return XGE_OK;
    started=1; xide_app app={0}; char** argv=user; const char* root=argv[1];
    char* state=xidePathResolve(root,".xlang-update");char* receipt=xidePathResolve(root,".xlang-update/installed.json");
    CHECK(state && receipt && xideIoCreateDirectories(state));
    CHECK(!strcmp(xideProgramVersion(),argv[2]));
    CHECK(xideInstalledIterationAt(NULL)==0 && xideInstalledIterationAt(state)==0 && !xrtGetError());
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
    CHECK(xideAppCreate(&app)==XUI_OK && settle(&app));
    const xide_action_info* action=xideActionNamed("help.about");CHECK(action && action->id==XIDE_ACTION_ABOUT);
    CHECK(xideActionExecute(&app,XIDE_ACTION_ABOUT)==XIDE_ACTION_DISPATCHED && settle(&app));
    CHECK(xideAboutViewIsOpen(&app) && xideActionsHasModal(&app));
    CHECK(xuiWidgetGetType(xideAboutViewWindow(&app))==xuiWindowGetType(app.context));
    CHECK(!xideActionQuery(&app,XIDE_ACTION_NEW_FILE).enabled);
    CHECK(click(&app,xideAboutViewCloseButton(&app)) && !xideActionsHasModal(&app));
    CHECK(xideActionExecute(&app,XIDE_ACTION_ABOUT)==XIDE_ACTION_DISPATCHED && settle(&app));
    CHECK(xuiGetFocusWidget(app.context)==xideAboutViewCloseButton(&app));
    CHECK(xuiInputKeyDown(app.context,XUI_KEY_ESCAPE,0)==XUI_OK && xuiInputKeyUp(app.context,XUI_KEY_ESCAPE,0)==XUI_OK && settle(&app));
    CHECK(!xideAboutViewIsOpen(&app) && xideActionQuery(&app,XIDE_ACTION_NEW_FILE).enabled);
    CHECK(xideActionExecute(&app,XIDE_ACTION_ABOUT)==XIDE_ACTION_DISPATCHED && settle(&app));
    CHECK(click(&app,xuiWindowGetCloseButtonWidget(xideAboutViewWindow(&app))) && !xideAboutViewIsOpen(&app));
    CHECK(xideActionExecute(&app,XIDE_ACTION_ABOUT)==XIDE_ACTION_DISPATCHED && settle(&app));
    app.viewportWidth=640;app.viewportHeight=480;CHECK(settle(&app));
    xui_rect_t r=xuiWidgetGetRect(xideAboutViewWindow(&app));CHECK(r.fW<=640 && r.fH<=480);
    xideUpdateApplyCheckResult(&app,4,0);CHECK(xideUpdateIsRequired(&app));
    /* Small viewports can scroll the content; the title close stays visible. */
    CHECK(click(&app,xuiWindowGetCloseButtonWidget(xideAboutViewWindow(&app))) && !xideAboutViewIsOpen(&app));
    CHECK(xideUpdateIsRequired(&app) && xideActionsHasModal(&app) && !xideActionQuery(&app,XIDE_ACTION_ABOUT).enabled);
    xideUpdateApplyCheckResult(&app,0,0);CHECK(!xideActionsHasModal(&app));
    puts("PASS native XUI about action, close/Escape/X, small-window bounds, modal cleanup, required-update gate and safe installed-iteration fallback");
cleanup:
    xideAppDestroy(&app);xgeSetQuitRequestCallback(NULL,NULL);free(state);free(receipt);xgeQuit();return XGE_OK;
}
int main(int argc,char** argv)
{
    if(argc!=3) return 2;
    xge_desc_t d={0};d.iWidth=1280;d.iHeight=800;d.sTitle="xlang about window test";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&d);if(result==XGE_OK) result=xgeRun(verify,argv);xgeUnit();return result==XGE_OK && started && !failed ? 0 : 1;
}
