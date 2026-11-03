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
static int click(xide_app* a,xui_widget w){xui_rect_t r=xuiWidgetGetWorldRect(w);return xuiInputPointerMove(a->context,r.fX+r.fW/2,r.fY+r.fH/2,0)==XUI_OK&&xuiInputPointerDown(a->context,r.fX+r.fW/2,r.fY+r.fH/2,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK&&xuiInputPointerUp(a->context,r.fX+r.fW/2,r.fY+r.fH/2,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK&&settle(a);}
static int input(xide_app* a,int type,int key,uint32_t text){xge_input_event_t e={0};e.iSize=sizeof(e);e.iType=type;e.iKey=key;e.iCodepoint=text;e.iFlags=XGE_INPUT_EVENT_FLAG_SYNTHETIC;return xgeInputEventPost(&e)==XGE_OK&&xidePlatformHandleInput(a)==XUI_OK&&settle(a);}
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
    xideUpdateApplyCheckResult(&app,2,0);xideUpdatePoll(&app);CHECK(!xideUpdateCurrentPrompt(&app));
    xideUpdateApplyCheckResult(&app,2,1);xideUpdatePoll(&app);CHECK(settle(&app));box=xideUpdateCurrentPrompt(&app);CHECK(box&&!xideUpdateIsRequired(&app));
    CHECK(click(&app,xuiMsgBoxGetButtonWidget(box,1)));xideUpdatePoll(&app);CHECK(!xideUpdateIsModal(&app));
    uint64_t before=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor));
    CHECK(click(&app,doc->editor)&&input(&app,XGE_EVENT_TEXT,0,'R')&&xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(doc->editor))>before);
    CHECK(input(&app,XGE_EVENT_KEY_DOWN,XGE_KEY_BACKSPACE,0)&&input(&app,XGE_EVENT_KEY_UP,XGE_KEY_BACKSPACE,0));
    CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"func unsaved()\nend\n"));
    xideUpdateApplyCheckResult(&app,4,0);xideUpdatePoll(&app);CHECK(settle(&app));box=xideUpdateCurrentPrompt(&app);
    CHECK(box&&xideUpdateIsRequired(&app)&&xideActionsHasModal(&app)&&!xideActionQuery(&app,XIDE_ACTION_BUILD).enabled);
    CHECK(input(&app,XGE_EVENT_KEY_DOWN,XGE_KEY_ESCAPE,0)&&input(&app,XGE_EVENT_KEY_UP,XGE_KEY_ESCAPE,0));
    CHECK(xideUpdateIsModal(&app)&&xideUpdateIsRequired(&app)&&xuiMsgBoxIsOpen(box));
    CHECK(click(&app,doc->editor)&&input(&app,XGE_EVENT_TEXT,0,'Q'));
    CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"func unsaved()\nend\n"));
    xui_rect_i_t full={0,0,app.viewportWidth,app.viewportHeight};char* picture=xidePathResolve(directory,"required-update.png");
    CHECK(picture&&xuiUpdate(app.context,0)==XUI_OK&&app.proxy.surfaceClear(&app.proxy,app.target,0xffeeeeeeu)==XUI_OK&&xuiRender(app.context,app.target,&full,1)==XUI_OK&&xideAppCapture(&app,picture)==XUI_OK);free(picture);
    CHECK(click(&app,xuiMsgBoxGetButtonWidget(box,0))&&xideMessageBusy(&app)&&xideUpdatePending(&app));
    CHECK(click(&app,xuiMsgBoxGetButtonWidget(xideMessageCurrent(&app),2)));xideUpdatePoll(&app);CHECK(settle(&app));
    CHECK(doc->dirty&&!xideUpdatePending(&app)&&xideUpdateIsRequired(&app)&&xideUpdateCurrentPrompt(&app));
    box=xideUpdateCurrentPrompt(&app);CHECK(click(&app,xuiMsgBoxGetButtonWidget(box,1))&&xideMessageBusy(&app));
    CHECK(click(&app,xuiMsgBoxGetButtonWidget(xideMessageCurrent(&app),2)));xideUpdatePoll(&app);CHECK(settle(&app)&&xideUpdateIsRequired(&app)&&xideUpdateIsModal(&app));
    xideUpdateApplyCheckResult(&app,1,0);xideUpdatePoll(&app);CHECK(xideUpdateIsRequired(&app));
    xideUpdateApplyCheckResult(&app,0,0);xideUpdatePoll(&app);CHECK(!xideUpdateIsRequired(&app)&&!xideUpdateIsModal(&app)&&doc->dirty);
    puts("PASS update action registry, corrupt-preference fallback/preservation, and unsaved-document cancel handoff");
    puts("PASS optional dismissal, mandatory action/input/escape gate, update/exit save cancellation, offline retention and signed-policy release");
cleanup:
    xideSettingsClear(&prefs);free(bad);xideAppDestroy(&app);xgeSetQuitRequestCallback(NULL,NULL);xgeQuit();return XGE_OK;
}
int main(int argc,char** argv){if(argc!=2)return 2;xge_desc_t d={0};d.iWidth=1280;d.iHeight=800;d.sTitle="xlang update handoff test";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(verify,argv[1]);xgeUnit();return result==XGE_OK&&started&&!failed?0:1;}
