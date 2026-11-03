#include "app/xide_app.h"
#include "services/update_client.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failed,started;
#define CHECK(e) do{if(!(e)){fprintf(stderr,"update_client_test:%d %s\n",__LINE__,#e);failed=1;goto cleanup;}}while(0)
static int settle(xide_app* a){if(xuiDispatchPendingEvents(a->context)!=XUI_OK)return 0;xideMessagePoll(a);xideEditorPoll(a);xideWorkbenchLayout(a);return xuiLayout(a->context)==XUI_OK;}
static int verify(void* user){if(started)return XGE_OK;started=1;const char* directory=user;xide_app app={0};xide_model_error error={0};xide_settings prefs={0};char* bad=xidePathResolve(directory,"bad-settings.json");
    CHECK(bad&&xrtFileWriteAtomic(bad,(xbytesview){(const unsigned char*)"{broken",7}));
    CHECK(xideSettingsLoad(&prefs,bad,&error)&&prefs.source&&prefs.editorFontSize==16&&prefs.uiFontSize==16);
    size_t n=0;unsigned char* bytes=xrtFileReadAll(bad,&n);CHECK(bytes&&n==7&&!memcmp(bytes,"{broken",7));xrtFree(bytes);
    app.hidden=1;app.noSession=1;app.disableAutomaticDiagnostics=1;CHECK(xideAppCreate(&app)==XUI_OK);app.hidden=0;
    app.preferences.source=xrtJsonParse((xstrview){"{\"update\":{\"enabled\":false}}",28});CHECK(app.preferences.source);
    CHECK(xideActionNamed("help.checkUpdates")&&xideActionNamed("help.repairInstallation"));
    xide_document* doc=xideDocumentNew(&app.documents,"xlang",&error);CHECK(doc&&xideEditorShow(&app,doc)==XUI_OK);
    CHECK(xuiCodeEditSetText(doc->editor,"func unsaved()\nend\n")==XUI_OK&&settle(&app)&&doc->dirty);
    CHECK(xideUpdateAction(&app,XIDE_ACTION_UPDATE_REPAIR)==XUI_OK&&xideUpdatePending(&app));
    CHECK(xideMessageCurrent(&app)&&xideMessageBusy(&app)&&doc->dirty&&settle(&app));
    xui_msgbox box=xideMessageCurrent(&app);CHECK(xuiMsgBoxGetButtonCount(box)==3);
    xui_widget cancel=xuiMsgBoxGetButtonWidget(box,2);xui_rect_t r=xuiWidgetGetWorldRect(cancel);
    CHECK(xuiInputPointerMove(app.context,r.fX+r.fW/2,r.fY+r.fH/2,0)==XUI_OK&&xuiInputPointerDown(app.context,r.fX+r.fW/2,r.fY+r.fH/2,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK&&xuiInputPointerUp(app.context,r.fX+r.fW/2,r.fY+r.fH/2,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK&&settle(&app));
    xideUpdatePoll(&app);CHECK(doc->dirty&&!xideUpdatePending(&app)&&!xideMessageBusy(&app));
    puts("PASS update action registry, corrupt-preference fallback/preservation, and unsaved-document cancel handoff");
cleanup:
    xideSettingsClear(&prefs);free(bad);xideAppDestroy(&app);xgeSetQuitRequestCallback(NULL,NULL);xgeQuit();return XGE_OK;
}
int main(int argc,char** argv){if(argc!=2)return 2;xge_desc_t d={0};d.iWidth=1280;d.iHeight=800;d.sTitle="xlang update handoff test";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(verify,argv[1]);xgeUnit();return result==XGE_OK&&started&&!failed?0:1;}
