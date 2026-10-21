#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "ui/configuration_view.h"
#include "ui/catalog_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"input_test:%d: %s; %s\n",__LINE__,#e,error.message); failed=1; goto cleanup; } } while(0)
static int verificationFailed,verificationStarted;
typedef struct input_log { int composition, middleDown, middleUp; char preedit[64]; } input_log;
static int post(int type,int key,uint32_t mods,uint32_t codepoint,const char* text,int repeat)
{
    xge_input_event_t event={0}; event.iSize=sizeof(event); event.iType=type;
    event.iKey=key; event.iModifiers=mods; event.iCodepoint=codepoint;
    event.iFlags=XGE_INPUT_EVENT_FLAG_SYNTHETIC | (repeat ? XGE_INPUT_EVENT_FLAG_REPEAT : 0u);
    event.sText=text; event.iTextSize=text ? (int)strlen(text) : 0;
    event.iCursor=event.iSelectStart=event.iSelectEnd=event.iTextSize;
    int ret=xgeInputEventPost(&event); if(ret!=XGE_OK) fprintf(stderr,"post type=%d key=%d returned %d\n",type,event.iKey,ret); return ret==XGE_OK;
}
static int chord(int key,uint32_t mods) { return post(XGE_EVENT_KEY_DOWN,key,mods,0,NULL,0) && post(XGE_EVENT_KEY_UP,key,mods,0,NULL,0); }
static int pointer(int type,int x,int y,int button,uint32_t buttons,uint64_t id)
{
    xge_input_event_t event={0}; event.iSize=sizeof(event); event.iType=type;
    event.iFlags=XGE_INPUT_EVENT_FLAG_SYNTHETIC; event.fX=(float)x; event.fY=(float)y;
    event.iButton=button; event.iButtons=buttons; event.iPointerId=id;
    int ret=xgeInputEventPost(&event); if(ret!=XGE_OK) fprintf(stderr,"post type=%d key=%d returned %d\n",type,event.iKey,ret); return ret==XGE_OK;
}
static int pump(xide_app* app)
{
    int ret=xidePlatformHandleInput(app); if(ret!=XUI_OK) { fprintf(stderr,"pump returned %d\n",ret); return 0; }
    xideEditorPoll(app); xideWorkbenchLayout(app);
    ret=xuiLayout(app->context); if(ret!=XUI_OK || app->frameError) fprintf(stderr,"layout=%d frameError=%d\n",ret,app->frameError); return ret==XUI_OK && !app->frameError;
}
static xide_document* newDocument(xide_app* app,const char* language,xide_model_error* error)
{
    xide_document* doc=xideDocumentNew(&app->documents,language,error);
    return doc && xideEditorShow(app,doc)==XUI_OK && pump(app) ? doc : NULL;
}
static int observeEdit(xui_widget widget,const xui_edit_event_t* event,void* user)
{
    (void)widget; input_log* log=user;
    if(event->iType==XUI_EDIT_EVENT_COMPOSITION_CHANGED) {
        ++log->composition; snprintf(log->preedit,sizeof(log->preedit),"%.*s",event->iTextSize,event->sText ? event->sText : "");
    }
    return XUI_OK;
}
static int observePointer(xui_widget widget,const xui_event_t* event,void* user)
{
    (void)widget; input_log* log=user;
    if(event->iPhase==XUI_EVENT_PHASE_CAPTURE && event->iButton==XUI_POINTER_BUTTON_MIDDLE) {
        if(event->iType==XUI_EVENT_POINTER_DOWN) ++log->middleDown;
        if(event->iType==XUI_EVENT_POINTER_UP) ++log->middleUp;
    }
    return XUI_OK;
}
static int render(xide_app* app,const char* path)
{
    xui_rect_i_t full={0,0,app->viewportWidth,app->viewportHeight};
    size_t size=(size_t)full.iW*(size_t)full.iH*4;
    unsigned char* pixels=malloc(size); if(!pixels) return 0;
    int ret=xuiUpdate(app->context,0.0f);
    if(ret==XUI_OK) ret=app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu);
    if(ret==XUI_OK) ret=xuiRender(app->context,app->target,&full,1);
    if(ret==XUI_OK) ret=app->proxy.surfaceReadRGBA(&app->proxy,app->target,pixels,full.iW*4);
    if(ret==XUI_OK) ret=xgeImageSavePNG(path,full.iW,full.iH,pixels,full.iW*4);
    free(pixels); return ret==XGE_OK;
}
static int verify(void* user)
{
    char** argv=user; verificationStarted=1;
    xide_app app={0}; xide_model_error error={0}; input_log log={0}; int failed=0,began=0;
    char *root=NULL,*image=NULL,*entry=NULL,*projectPath=NULL;
    root=xidePathResolve(argv[1],"中文 输入"); CHECK(root && xrtDirCreateAll(root));
    image=xidePathResolve(root,"native-ime-preedit.png"); entry=xidePathResolve(root,"main.xl"); projectPath=xidePathResolve(root,"input.xlp");
    CHECK(image && entry && projectPath);
    app.hidden=1; app.noSession=1; app.maxFrames=1;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeImeGetMode()==XGE_IME_MODE_COMPOSITION);
    CHECK(xgeBegin()==XGE_OK); began=1;
    CHECK(pump(&app));
    uint64_t dropped=xgeInputEventDroppedCount();
    xide_document* doc=newDocument(&app,"xlang",&error); CHECK(doc);
    xui_code_document core=xuiCodeEditGetDocument(doc->editor);
    /* All three events arrive in one frame: text, delete, text. */
    CHECK(post(XGE_EVENT_TEXT,0,0,'A',NULL,0) && chord(XGE_KEY_BACKSPACE,0) && post(XGE_EVENT_TEXT,0,0,'B',NULL,0) && pump(&app));
    CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"B"));
    /* Modifiers belong to each event, not the final keyboard snapshot. */
    CHECK(post(XGE_EVENT_TEXT,0,0,'a',NULL,0) && post(XGE_EVENT_TEXT,0,XGE_KEY_MOD_CTRL,'Q',NULL,0) && post(XGE_EVENT_TEXT,0,0,'b',NULL,0) && pump(&app));
    CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"Bab"));
    CHECK(post(XGE_EVENT_KEY_DOWN,XGE_KEY_BACKSPACE,0,0,NULL,0) && post(XGE_EVENT_KEY_DOWN,XGE_KEY_BACKSPACE,0,0,NULL,1) && post(XGE_EVENT_KEY_UP,XGE_KEY_BACKSPACE,0,0,NULL,0) && pump(&app));
    CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"B"));
    CHECK(chord('Z',XGE_KEY_MOD_CTRL) && pump(&app)); CHECK(strcmp(xuiCodeEditGetText(doc->editor),"B"));
    CHECK(chord('Y',XGE_KEY_MOD_CTRL) && pump(&app)); CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"B"));
    CHECK(chord('N',XGE_KEY_MOD_CTRL) && post(XGE_EVENT_TEXT,0,0,0x4e2d,NULL,0) && pump(&app));
    CHECK(app.activeDocument!=doc && !strcmp(xuiCodeEditGetText(app.activeDocument->editor),"中") && !strcmp(xuiCodeEditGetText(doc->editor),"B"));
    doc=newDocument(&app,"c",&error); CHECK(doc); core=xuiCodeEditGetDocument(doc->editor);
    CHECK(post(XGE_EVENT_TEXT,0,0,'r',NULL,0) && post(XGE_EVENT_TEXT,0,0,'e',NULL,0) && post(XGE_EVENT_TEXT,0,0,'t',NULL,0) && chord(XGE_KEY_SPACE,XGE_KEY_MOD_CTRL) && pump(&app));
    CHECK(xuiCodeEditIsCompletionOpen(doc->editor) && xuiCodeEditGetCompletionCount(doc->editor)>0);
    CHECK(chord(XGE_KEY_SPACE,XGE_KEY_MOD_CTRL) && pump(&app) && xuiCodeEditIsCompletionOpen(doc->editor));
    CHECK(chord(XGE_KEY_ENTER,0) && pump(&app) && !strcmp(xuiCodeEditGetText(doc->editor),"return"));
    CHECK(chord('Z',XGE_KEY_MOD_CTRL) && pump(&app) && !strcmp(xuiCodeEditGetText(doc->editor),"ret"));
    CHECK(xideEditorShow(&app,doc)==XUI_OK && pump(&app)); /* Rebinding is idempotent. */
    uint32_t cVersion=xuiCodeDocumentGetVersion(core);
    CHECK(post(XGE_EVENT_IME_START,0,0,0,NULL,0) && post(XGE_EVENT_IME_UPDATE,0,0,0,"中文",0) && post(XGE_EVENT_IME_COMMIT,0,0,0," // 中文😀",0) && post(XGE_EVENT_IME_END,0,0,0,NULL,0) && pump(&app));
    CHECK(!strcmp(doc->language,"c") && !strcmp(xuiCodeEditGetText(doc->editor),"ret // 中文😀") && xuiCodeDocumentGetVersion(core)==cVersion+1);
    CHECK(chord('Z',XGE_KEY_MOD_CTRL) && pump(&app) && !strcmp(xuiCodeEditGetText(doc->editor),"ret"));
    doc=newDocument(&app,"xlang",&error); CHECK(doc); core=xuiCodeEditGetDocument(doc->editor);
    CHECK(xuiEditSetEvent(doc->editor,observeEdit,&log)==XUI_OK);
    uint32_t version=xuiCodeDocumentGetVersion(core);
    char composition[]="中文组合";
    CHECK(post(XGE_EVENT_IME_START,0,0,0,NULL,0) && post(XGE_EVENT_IME_UPDATE,0,0,0,composition,0));
    memset(composition,'x',strlen(composition)); /* Posting must own its payload. */
    CHECK(post(XGE_EVENT_TEXT,0,0,'Z',NULL,0) && pump(&app));
    CHECK(log.composition>=2 && !strcmp(log.preedit,"中文组合") && !doc->dirty && version==xuiCodeDocumentGetVersion(core) && !*xuiCodeEditGetText(doc->editor));
    CHECK(!strstr(xuiDockPanelGetWindowTitle(app.dock,doc->window)," *"));
    CHECK(render(&app,image));
    CHECK(post(XGE_EVENT_IME_COMMIT,0,0,0,"中文😀",0) && post(XGE_EVENT_IME_END,0,0,0,NULL,0) && pump(&app));
    CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"中文😀") && doc->dirty && xuiCodeDocumentGetVersion(core)==version+1);
    CHECK(chord('Z',XGE_KEY_MOD_CTRL) && pump(&app) && !*xuiCodeEditGetText(doc->editor));
    CHECK(chord('Y',XGE_KEY_MOD_CTRL) && pump(&app) && !strcmp(xuiCodeEditGetText(doc->editor),"中文😀"));
    version=xuiCodeDocumentGetVersion(core);
    CHECK(post(XGE_EVENT_IME_START,0,0,0,NULL,0) && post(XGE_EVENT_IME_UPDATE,0,0,0,"取消",0) && post(XGE_EVENT_IME_END,0,0,0,NULL,0) && pump(&app));
    CHECK(version==xuiCodeDocumentGetVersion(core) && !strcmp(xuiCodeEditGetText(doc->editor),"中文😀"));
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(doc->editor),core,xuiCodeDocumentGetLength(core),0)==XUI_OK);
    CHECK(post(XGE_EVENT_IME_START,0,0,0,NULL,0) && post(XGE_EVENT_IME_UPDATE,0,0,0,"未提交",0) && pump(&app));
    CHECK(xuiSetFocusWidget(app.context,app.commands)==XUI_OK && post(XGE_EVENT_IME_END,0,0,0,NULL,0) && pump(&app));
    CHECK(version==xuiCodeDocumentGetVersion(core) && !strcmp(xuiCodeEditGetText(doc->editor),"中文😀"));
    CHECK(xuiSetFocusWidget(app.context,doc->editor)==XUI_OK && post(XGE_EVENT_TEXT,0,0,'!',NULL,0) && pump(&app));
    CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"中文😀!"));
    CHECK(chord('Z',XGE_KEY_MOD_CTRL) && pump(&app));
    CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"中文😀"));
    /* Native replacement ranges are UTF-8 bytes, including non-BMP text. */
    xge_input_event_t replacement={0}; replacement.iSize=sizeof(replacement); replacement.iType=XGE_EVENT_IME_COMMIT;
    replacement.iFlags=XGE_INPUT_EVENT_FLAG_SYNTHETIC; replacement.sText="汉字"; replacement.iTextSize=6;
    replacement.iCursor=replacement.iSelectStart=replacement.iSelectEnd=6;
    replacement.bReplacementRange=1; replacement.iReplacementStart=0; replacement.iReplacementEnd=6;
    CHECK(xgeInputEventPost(&replacement)==XGE_OK && pump(&app));
    CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"汉字😀"));
    CHECK(chord('Z',XGE_KEY_MOD_CTRL) && pump(&app) && !strcmp(xuiCodeEditGetText(doc->editor),"中文😀"));
    CHECK(xuiCodeEditSetReadonly(doc->editor,1)==XUI_OK);
    version=xuiCodeDocumentGetVersion(core);
    CHECK(post(XGE_EVENT_IME_START,0,0,0,NULL,0) && post(XGE_EVENT_IME_UPDATE,0,0,0,"只读",0) && post(XGE_EVENT_IME_COMMIT,0,0,0,"只读",0) && post(XGE_EVENT_IME_END,0,0,0,NULL,0) && pump(&app));
    CHECK(version==xuiCodeDocumentGetVersion(core) && !strcmp(xuiCodeEditGetText(doc->editor),"中文😀"));
    CHECK(xuiCodeEditSetReadonly(doc->editor,0)==XUI_OK && xuiWidgetSetEventCallback(app.root,observePointer,&log)==XUI_OK);
    xui_rect_t rect=xuiWidgetGetWorldRect(doc->editor); int x=rect.fX+120,y=rect.fY+45;
    CHECK(pointer(XGE_EVENT_MOUSE_DOWN,x,y,XGE_MOUSE_MIDDLE,XGE_MOUSE_MIDDLE,0) && pointer(XGE_EVENT_MOUSE_UP,x,y,XGE_MOUSE_MIDDLE,0,0) && pump(&app));
    CHECK(log.middleDown==1 && log.middleUp==1);
    CHECK(xuiSetPointerCapture(app.context,doc->editor)==XUI_OK && xuiSetPointerCaptureEx(app.context,42,XUI_POINTER_TYPE_TOUCH,doc->editor)==XUI_OK);
    CHECK(pointer(XGE_EVENT_TOUCH_CANCEL,x,y,0,0,42) && pump(&app) && !xuiGetPointerCaptureEx(app.context,42,XUI_POINTER_TYPE_TOUCH));
    /* These event kinds cannot be posted synthetically in the pinned SDK.
       Exercise the real Win32 event route instead. */
    SendMessageW((HWND)xgePlatformNativeHandle(),WM_KILLFOCUS,0,0);
    CHECK(pump(&app) && !xuiGetPointerCapture(app.context));
    SendMessageW((HWND)xgePlatformNativeHandle(),WM_SETFOCUS,0,0);
    CHECK(pump(&app));
    CHECK(xuiDockPanelFloatWindow(app.dock,doc->window,(xui_rect_t){390,100,530,340})==XUI_OK && xideEditorShow(&app,doc)==XUI_OK && pump(&app));
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(doc->editor),core,xuiCodeDocumentGetLength(core),0)==XUI_OK);
    CHECK(post(XGE_EVENT_IME_START,0,0,0,NULL,0) && post(XGE_EVENT_IME_UPDATE,0,0,0,"浮动",0) && pump(&app));
    xge_rect_t candidate={0}; rect=xuiWidgetGetWorldRect(doc->editor);
    CHECK(xgeImeGetCandidateRect(&candidate)==XGE_OK && candidate.fH>0 && candidate.fX>=rect.fX && candidate.fX<=rect.fX+rect.fW && candidate.fY>=rect.fY && candidate.fY<=rect.fY+rect.fH);
    CHECK(xuiSetFocusWidget(app.context,app.commands)==XUI_OK && post(XGE_EVENT_IME_END,0,0,0,NULL,0) && pump(&app));
    CHECK(xuiSetFocusWidget(app.context,doc->editor)==XUI_OK && post(XGE_EVENT_TEXT,0,0,'?',NULL,0) && pump(&app));
    CHECK(!strcmp(xuiCodeEditGetText(doc->editor),"中文😀?") && chord('Z',XGE_KEY_MOD_CTRL) && pump(&app) && !strcmp(xuiCodeEditGetText(doc->editor),"中文😀"));
    CHECK(xideWorkbenchReset(&app)==XUI_OK && pump(&app));
    /* A click and following text must target the new focused input. */
    const char* source="func main(...) as int\n    return 0\nend\n";
    CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)source,strlen(source)}));
    xide_project project={0}; CHECK(xideProjectCreate(&project,projectPath,"输入测试",entry,&error));
    int saved=xideProjectSave(&project,NULL,&error); xideProjectClear(&project); CHECK(saved);
    CHECK(xideWorkspaceCreate(&app.workspaceModel,"输入工作空间",&error) && xideWorkspaceAddProject(&app.workspaceModel,projectPath,"input",&error));
    CHECK(xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,"input") && xideConfigurationViewOpen(&app) && pump(&app));
    xui_widget name=xideConfigurationViewControl(&app,XIDE_CONFIG_NAME);
    xui_widget output=xideConfigurationViewControl(&app,XIDE_CONFIG_OUTPUT);
    char outputBefore[512]; snprintf(outputBefore,sizeof(outputBefore),"%s",xuiInputGetText(output));
    CHECK(xuiEditSetText(name,"")==XUI_OK && xuiSetFocusWidget(app.context,output)==XUI_OK);
    rect=xuiWidgetGetWorldRect(name);
    CHECK(pointer(XGE_EVENT_MOUSE_DOWN,rect.fX+rect.fW/2,rect.fY+rect.fH/2,XGE_MOUSE_LEFT,XGE_MOUSE_LEFT,0) && pointer(XGE_EVENT_MOUSE_UP,rect.fX+rect.fW/2,rect.fY+rect.fH/2,XGE_MOUSE_LEFT,0,0));
    CHECK(post(XGE_EVENT_IME_START,0,0,0,NULL,0) && post(XGE_EVENT_IME_UPDATE,0,0,0,"配置",0) && post(XGE_EVENT_IME_COMMIT,0,0,0,"中文配置",0) && post(XGE_EVENT_IME_END,0,0,0,NULL,0) && pump(&app));
    CHECK(xuiGetFocusWidget(app.context)==name && !strcmp(xuiInputGetText(name),"中文配置") && !strcmp(xuiInputGetText(output),outputBefore));
    xide_document* active=app.activeDocument; size_t count=app.documents.count;
    CHECK(chord('N',XGE_KEY_MOD_CTRL) && chord('W',XGE_KEY_MOD_CTRL) && chord(XGE_KEY_F7,0) && pump(&app));
    CHECK(xideConfigurationViewIsOpen(&app) && app.activeDocument==active && app.documents.count==count && !strcmp(xuiInputGetText(name),"中文配置"));
    xui_widget cancel=xideConfigurationViewControl(&app,XIDE_CONFIG_CANCEL); rect=xuiWidgetGetWorldRect(cancel);
    CHECK(pointer(XGE_EVENT_MOUSE_DOWN,rect.fX+rect.fW/2,rect.fY+rect.fH/2,XGE_MOUSE_LEFT,XGE_MOUSE_LEFT,0) && pointer(XGE_EVENT_MOUSE_UP,rect.fX+rect.fW/2,rect.fY+rect.fH/2,XGE_MOUSE_LEFT,0,0) && pump(&app));
    CHECK(!xideConfigurationViewIsOpen(&app));
    CHECK(xideEditorOpen(&app,entry,&error) && pump(&app));
    xide_document* savedDoc=app.activeDocument;
    CHECK(chord(XGE_KEY_END,XGE_KEY_MOD_CTRL) && post(XGE_EVENT_TEXT,0,0,'/',NULL,0) && post(XGE_EVENT_TEXT,0,0,'/',NULL,0) && post(XGE_EVENT_TEXT,0,0,'M',NULL,0) && chord('S',XGE_KEY_MOD_CTRL) && pump(&app));
    CHECK(!savedDoc->dirty);
    xide_text_file savedText={0}; CHECK(xideTextFileRead(entry,&savedText,&error));
    int matches=!strcmp(savedText.text,xuiCodeEditGetText(savedDoc->editor)) && strstr(savedText.text,"//M");
    xideTextFileClear(&savedText); CHECK(matches);
    CHECK(chord('W',XGE_KEY_MOD_CTRL) && pump(&app) && !savedDoc->open);
    CHECK(xgeInputEventPendingCount()==0 && xgeInputEventDroppedCount()==dropped && !app.frameError);
    puts("input_test passed: native ordered text/key/focus, event modifiers, repeat, C completion once, Unicode IME preedit/commit/cancel/replacement/undo/read-only, input field/floating editor composition and candidate anchor, middle button, touch cancel, blur capture, modal shortcuts and ordered save/close.");
cleanup:
    if(began) xgeEnd();
    xideAppDestroy(&app); xgeSetQuitRequestCallback(NULL,NULL);
    free(root); free(image); free(entry); free(projectPath);
    verificationFailed=failed; xgeQuit(); return XGE_OK;
}
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    xge_desc_t desc={0}; desc.iWidth=1280; desc.iHeight=800;
    desc.sTitle="xlang IDE input verification";
    desc.iFlags=XGE_INIT_WINDOW; desc.iRunMode=XGE_RUN_GAME_LOOP;
    int ret=xgeInit(&desc);
    if(ret==XGE_OK) ret=xgeRun(verify,argv);
    xgeUnit(); return ret==XGE_OK && verificationStarted && !verificationFailed ? 0 : 1;
}
