#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/template_view.h"
#include "ui/goto_view.h"
#include "ui/font_view.h"
#include "ui/editor_view.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef struct fixture { const char* directory;HDESK desktop;int started,failed,checks; } fixture;
#define CHECK(e) do { ++f->checks;if(!(e)) { fprintf(stderr,"dialog_message_test:%d: %s; %s\n",__LINE__,#e,error.message);f->failed=1;goto cleanup; } } while(0)
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0;xideMessagePoll(a);xideEditorPoll(a);xideWorkbenchLayout(a);return xuiLayout(a->context)==XUI_OK; }
static int point(xide_app* a,float x,float y) { return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int click(xide_app* a,xui_widget w) { xui_rect_t r=xuiWidgetGetWorldRect(w);return point(a,r.fX+r.fW/2,r.fY+r.fH/2); }
static int key(xide_app* a,int k) { return xuiInputKeyDownEx(a->context,k,0,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,0,NULL)==XUI_OK && settle(a); }
static int capture(xide_app* a,const char* path) { xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight};return xuiUpdate(a->context,0)==XUI_OK && a->proxy.surfaceClear(&a->proxy,a->target,0)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int reply(xide_app* a,int button) { xui_msgbox box=xideMessageCurrent(a);return box && settle(a) && click(a,xuiMsgBoxGetButtonWidget(box,button)); }
static int verify(void* user) {
    fixture* f=user;if(f->started) return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0;char *source=NULL,*image=NULL,*settings=NULL; xide_text_file disk={0};
    source=xidePathResolve(f->directory,"确认 中文.c");settings=xidePathResolve(f->directory,"settings.json");image=xidePathResolve(f->directory,"window.png");CHECK(source && image && settings && writeText(source,"int original;\n"));
    app.hidden=1;app.noSession=1;app.settingsPath=settings;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(settle(&app));
    CHECK(xideTemplateViewOpen(&app) && settle(&app) && !xgeRequestQuit());xui_widget window=xideTemplateViewWindow(&app);
    CHECK(xuiWidgetGetType(window)==xuiWindowGetType(app.context) && xuiWindowIsOpen(window) && !strcmp(xuiWindowGetTitle(window),"新建 xlang 项目"));
    CHECK(xuiEditSetText(xideTemplateViewControl(&app,XIDE_TEMPLATE_NAME),"窗口草稿")==XUI_OK && settle(&app));
    xui_rect_t before=xuiWidgetGetRect(window),title=xuiWindowGetTitleBarRect(window),world=xuiWidgetGetWorldRect(window);float x=world.fX+80,y=world.fY+title.fY+title.fH/2;
    CHECK(xuiInputPointerDown(app.context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && settle(&app));
    CHECK(xuiInputPointerMove(app.context,x+40,y+24,XUI_POINTER_BUTTON_LEFT)==XUI_OK && settle(&app));
    CHECK(xuiInputPointerUp(app.context,x+40,y+24,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(&app));
    xui_rect_t moved=xuiWidgetGetRect(window);CHECK(abs(moved.fX-before.fX)>15 && abs(moved.fY-before.fY)>10 && settle(&app));
    CHECK(abs(xuiWidgetGetRect(window).fX-moved.fX)<1 && !strcmp(xuiEditGetText(xideTemplateViewControl(&app,XIDE_TEMPLATE_NAME)),"窗口草稿"));
    /* Resize through the actual bottom/right grip, then run the normal layout. */
    x=moved.fX+moved.fW-2;y=moved.fY+moved.fH-2;
    CHECK(xuiInputPointerDown(app.context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && settle(&app));
    CHECK(xuiInputPointerMove(app.context,x-100,y-70,XUI_POINTER_BUTTON_LEFT)==XUI_OK && settle(&app));
    CHECK(xuiInputPointerUp(app.context,x-100,y-70,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(&app));
    xui_rect_t resized=xuiWidgetGetRect(window);CHECK(resized.fW<moved.fW-40 && resized.fH<moved.fH-30 && settle(&app) && abs(xuiWidgetGetRect(window).fW-resized.fW)<1);
    xui_rect_t scroll=xuiWidgetGetRect(xideDialogGetScrollViewWidget(window));CHECK(scroll.fW>resized.fW-50 && scroll.fH>resized.fH-80);
    CHECK(click(&app,xuiWindowGetMaximizeButtonWidget(window)) && xuiWindowIsMaximized(window) && settle(&app));
    CHECK(xuiWidgetGetRect(window).fW==app.viewportWidth && xuiWidgetGetRect(xideDialogGetScrollViewWidget(window)).fW>app.viewportWidth-50);
    CHECK(click(&app,xuiWindowGetMaximizeButtonWidget(window)) && !xuiWindowIsMaximized(window) && abs(xuiWidgetGetRect(window).fW-resized.fW)<1);
    CHECK(capture(&app,image) && click(&app,xuiWindowGetCloseButtonWidget(window)) && !xideTemplateViewIsOpen(&app) && !xideActionsHasModal(&app));
    CHECK(xideEditorOpen(&app,source,&error) && settle(&app));xide_document* d=app.activeDocument;
    CHECK(xuiCodeEditSetText(d->editor,"int edited;\n")==XUI_OK && settle(&app) && d->dirty);app.hidden=0;
    CHECK(xideActionExecute(&app,XIDE_ACTION_CLOSE_FILE)==XIDE_ACTION_DISPATCHED && settle(&app) && xideMessagePending(&app) && d->open);
    CHECK(xuiWidgetIsType(xuiMsgBoxGetWindowWidget(xideMessageCurrent(&app)),xuiWindowGetType(app.context)) && xideActionExecute(&app,XIDE_ACTION_NEW_FILE)==XIDE_ACTION_DISABLED);
    CHECK(reply(&app,2) && !xideMessageBusy(&app) && d->open && d->dirty); /* cancel */
    CHECK(xideActionExecute(&app,XIDE_ACTION_CLOSE_FILE)==XIDE_ACTION_DISPATCHED && settle(&app));
    CHECK(xuiCodeEditSetText(d->editor,"int changed_while_waiting;\n")==XUI_OK && reply(&app,1) && d->open && xideMessagePending(&app));
    CHECK(reply(&app,2) && d->open && d->dirty && !xideMessageBusy(&app));
    CHECK(xideActionExecute(&app,XIDE_ACTION_CLOSE_FILE)==XIDE_ACTION_DISPATCHED && settle(&app));
    xideMessageBeginAction(&app,XIDE_ACTION_STOP);xideMessageEndOperation(&app); /* allowed task control must preserve the suspended close */
    CHECK(xideMessagePending(&app) && reply(&app,1) && !d->open); /* no */
    CHECK(xideTextFileRead(source,&disk,&error) && !strcmp(disk.text,"int original;\n"));xideTextFileClear(&disk);
    CHECK(xideEditorOpen(&app,source,&error) && settle(&app));d=app.activeDocument;CHECK(xuiCodeEditSetText(d->editor,"int saved;\n")==XUI_OK && settle(&app));
    CHECK(xideActionExecute(&app,XIDE_ACTION_CLOSE_FILE)==XIDE_ACTION_DISPATCHED && settle(&app) && reply(&app,0) && !d->open && !xideMessageBusy(&app));
    CHECK(xideTextFileRead(source,&disk,&error) && !strcmp(disk.text,"int saved;\n"));xideTextFileClear(&disk);
    CHECK(xideEditorOpen(&app,source,&error) && settle(&app));d=app.activeDocument;CHECK(xuiCodeEditSetText(d->editor,"int local;\n")==XUI_OK && settle(&app) && writeText(source,"int external;\n"));
    CHECK(xideActionExecute(&app,XIDE_ACTION_SAVE_FILE)==XIDE_ACTION_DISPATCHED && settle(&app) && xideMessagePending(&app));
    CHECK(reply(&app,1) && d->dirty && xideTextFileRead(source,&disk,&error) && !strcmp(disk.text,"int external;\n"));xideTextFileClear(&disk);
    CHECK(xideActionExecute(&app,XIDE_ACTION_SAVE_FILE)==XIDE_ACTION_DISPATCHED && settle(&app));
    CHECK(writeText(source,"int changed_again;\n") && reply(&app,0) && xideMessagePending(&app)); /* stale disk approval must ask again */
    CHECK(xideTextFileRead(source,&disk,&error) && !strcmp(disk.text,"int changed_again;\n"));xideTextFileClear(&disk);
    CHECK(reply(&app,0) && !xideMessageBusy(&app) && !d->dirty && xideTextFileRead(source,&disk,&error) && !strcmp(disk.text,"int local;\n"));xideTextFileClear(&disk);
    CHECK(xuiCodeEditSetText(d->editor,"int discard;\n")==XUI_OK && settle(&app));CHECK(xideActionExecute(&app,XIDE_ACTION_RELOAD_FILE)==XIDE_ACTION_DISPATCHED && settle(&app));
    CHECK(click(&app,xuiWindowGetCloseButtonWidget(xuiMsgBoxGetWindowWidget(xideMessageCurrent(&app)))) && d->dirty && !xideMessageBusy(&app));
    CHECK(xideActionExecute(&app,XIDE_ACTION_RELOAD_FILE)==XIDE_ACTION_DISPATCHED && settle(&app) && reply(&app,0) && !d->dirty);
    xideMessageShow(&app,"错误边界","中文错误信息与长文档显示。",XUI_MSGBOX_ICON_ERROR);CHECK(settle(&app) && xideMessageBusy(&app));free(image);image=xidePathResolve(f->directory,"msgbox.png");CHECK(image && capture(&app,image) && reply(&app,0) && !xideMessageBusy(&app));
    CHECK(xideGotoViewOpen(&app) && settle(&app) && key(&app,XUI_KEY_ESCAPE) && !xideGotoViewIsOpen(&app));
    CHECK(xideUiFontApply(&app,app.uiFontPath,40,&error));
    xideMessageShow(&app,"大字体消息框","C 与 xlang 共用 IDE 设置，消息正文和按钮应完整显示。",XUI_MSGBOX_ICON_WAR);CHECK(settle(&app));
    CHECK(xuiMsgBoxGetButtonRect(xideMessageCurrent(&app),0).fH>60 && xuiWindowGetTitleBarRect(xuiMsgBoxGetWindowWidget(xideMessageCurrent(&app))).fH>60);
    free(image);image=xidePathResolve(f->directory,"msgbox-large.png");CHECK(image && capture(&app,image) && key(&app,XUI_KEY_ENTER) && !xideMessageBusy(&app));
    CHECK(xideUiFontApply(&app,app.uiFontPath,16,&error) && settle(&app));
    CHECK(xuiCodeEditSetText(d->editor,"int quit_unsaved;\n")==XUI_OK && settle(&app) && d->dirty);
    CHECK(!xgeRequestQuit() && settle(&app) && xideMessagePending(&app));
    CHECK(reply(&app,2) && d->open && d->dirty && !xideMessageBusy(&app));
    char* second=xidePathResolve(f->directory,"second.xl");CHECK(second && writeText(second,"func old()\nend\n") && xideEditorOpen(&app,second,&error) && settle(&app));
    xide_document* other=app.activeDocument;CHECK(xuiCodeEditSetText(other->editor,"func unsaved()\nend\n")==XUI_OK && settle(&app));
    xideWorkspaceClear(&app.workspaceModel);CHECK(xideWorkspaceCreate(&app.workspaceModel,"退出确认",&error));
    CHECK(!xgeRequestQuit() && settle(&app) && reply(&app,1) && xideMessagePending(&app));
    CHECK(d->dirty && other->dirty && reply(&app,1) && xideMessagePending(&app));
    CHECK(!strcmp(xuiMsgBoxGetTitle(xideMessageCurrent(&app)),"保存工作空间") && reply(&app,1) && !xideMessageBusy(&app));
    CHECK(xideTextFileRead(second,&disk,&error) && !strcmp(disk.text,"func old()\nend\n"));xideTextFileClear(&disk);free(second);
    printf("dialog_message_test passed: %d checks; actual XUI window drag/resize/maximize/restore/close/Esc, draft retention, cancel/no/yes, deferred action, document/disk approval baselines, quit sequence, reload and large-font error msgbox; three screenshots.\n",f->checks);
cleanup:
    xideTextFileClear(&disk);if(began) xgeEnd();xideAppDestroy(&app);free(source);free(image);free(settings);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user) { fixture* f=user;if(!SetThreadDesktop(f->desktop)) return 1;xge_desc_t d={0};d.iWidth=1280;d.iHeight=900;d.sTitle="XUI window and msgbox regression";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;int r=xgeInit(&d);if(r==XGE_OK) r=xgeRun(verify,f);xgeUnit();return r==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2;fixture f={.directory=argv[1]};wchar_t name[100];swprintf(name,100,L"XlangDialogVerification-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop) return 1;HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread); }CloseDesktop(f.desktop);return (int)code; }
