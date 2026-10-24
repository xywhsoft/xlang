#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/language_view.h"
#include "ui/task_view.h"
#include "ui/template_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/ui/completion_view.c"
typedef struct fixture {const char *directory,*compiler;HDESK desktop;int started,failed,checks,legacy;} fixture;
#define CHECK(test) do{++f->checks;if(!(test)){fprintf(stderr,"signature_view_test:%d %s; %s\n",__LINE__,#test,error.message);if(app.completionView){xide_completion_view* v=app.completionView;signature_view* s=&v->signature;fprintf(stderr,"sig wanted=%d pending=%d request=%d ready=%d supported=%d token=%llu epoch=%llu/%llu clientinit=%d sync=%llu completionwanted=%d pending=%d focusEditor=%d\n",s->wanted,s->pending,s->request,s->ready,s->supported,(unsigned long long)s->decodeToken,(unsigned long long)s->epoch,(unsigned long long)v->inputEpoch,v->initialized,(unsigned long long)v->syncToken,v->wanted,v->pending,app.activeDocument && xuiGetFocusWidget(app.context)==app.activeDocument->editor);}f->failed=1;goto cleanup;}}while(0)
static int settle(xide_app* a){if(xuiDispatchPendingEvents(a->context)!=XUI_OK)return 0;xideEditorPoll(a);xideWorkbenchLayout(a);return xuiLayout(a->context)==XUI_OK && !a->frameError;}
static int position(xide_app* a,xide_document* d,int at){return xideEditorShow(a,d)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),at,0)==XUI_OK && xuiSetFocusWidget(a->context,d->editor)==XUI_OK;}
static int key(xide_app* a,int k,uint32_t mods){return xuiInputKeyDownEx(a->context,k,mods,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,mods,NULL)==XUI_OK && settle(a);}
static int character(xide_app* a,uint32_t value){return xuiInputSetModifiers(a->context,0)==XUI_OK && xuiInputTextEx(a->context,value,NULL)==XUI_OK && settle(a);}
static int prepare(xide_app* a,xide_document* d,const char* source,const char* marker,int after){cvdismiss(a->completionView);svdismiss(a->completionView);const char* p=strstr(source,marker);return p && xuiCodeDocumentReplace(xuiCodeEditGetDocument(d->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor)),source)==XUI_OK && xideRefreshEditorSyntax(d->editor)==XUI_OK && position(a,d,(int)(p-source)+after) && settle(a);}
static int click(xide_app* a,xui_widget w){xui_rect_t r=xuiWidgetGetWorldRect(w);float x=r.fX+r.fW/2,y=r.fY+r.fH/2;return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a);}
static int waitHint(xide_app* a){uint64_t until=GetTickCount64()+20000;do{if(!settle(a))return 0;if(xideSignatureViewIsOpen(a))return 1;Sleep(2);}while(GetTickCount64()<until);return 0;}
static int idle(xide_app* a,int ms){uint64_t until=GetTickCount64()+ms;do{if(!settle(a))return 0;Sleep(2);}while(GetTickCount64()<until);return 1;}
static int capture(xide_app* a,const char* path){if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK || xuiLayout(a->context)!=XUI_OK)return 0;xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight};return a->proxy.surfaceClear(&a->proxy,a->target,ideThemeColor(a->theme,IDE_BG))==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK;}
static int verify(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0;char *entry=NULL,*picture=NULL;
 entry=xidePathResolve(f->directory,"signature.xl");picture=xidePathResolve(f->directory,"signature.png");const char* saved="func main(...) as int\n return 0\nend\n";
 CHECK(entry && picture && xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)saved,strlen(saved)}));
 app.hidden=1;app.noSession=1;app.disableAutomaticDiagnostics=1;app.compilerPath=f->compiler;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
 CHECK(xideEditorOpen(&app,entry,&error) && settle(&app));xide_document* d=app.activeDocument;
 const char* body="#doc\ndesc: 两个参数的示例说明\n#end\nfunc plus(int left,string right=\"\") as int\n return left\nend\nfunc main(...) as int\n return plus(7,\"x\")\nend\n";
 CHECK(xuiCodeEditSetText(d->editor,body)==XUI_OK && position(&app,d,(int)(strstr(body,"plus(7,")-body)+7));
 CHECK(xideActionQuery(&app,XIDE_ACTION_SIGNATURE).enabled && key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL|XUI_MOD_SHIFT));
 if(f->legacy){uint64_t until=GetTickCount64()+10000;while(!app.completionView->initialized && GetTickCount64()<until){CHECK(settle(&app));Sleep(2);}
  CHECK(app.completionView->initialized && !app.completionView->signature.supported && !app.completionView->signature.wanted && !xideSignatureViewIsOpen(&app) && app.completionView->nextId==1 && !app.frameError);
  printf("signature_view_test passed: %d checks; actual old0.4 service declines signature capability,no query/popup/error\n",f->checks);goto cleanup;
 }
 CHECK(waitHint(&app));
 signature_view* s=&app.completionView->signature;CHECK(s->result && s->result->valid && s->result->count==1 && !strcmp(xuiCodeEditGetText(s->preview),"plus(int left, string right = ...) as int"));
 CHECK(strstr(xuiTextEditGetText(s->documentation),"两个参数") && s->result->items[0].activeParameter==1 && xuiGetFocusWidget(app.context)==d->editor && !xideActionsHasModal(&app));
 xui_code_selection_t selected={0};CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(s->preview),&selected)==XUI_OK && selected.iAnchorOffset==15 && selected.iCaretOffset==33);
 {xui_rect_t box=xuiWidgetGetRect(s->preview),end={0};xuiCodeEditGetTextOffsetRect(s->preview,(int)strlen(xuiCodeEditGetText(s->preview)),&end);printf("Signature preview width=%d,height=%d,EOF=(%d,%d)\n",box.fW,box.fH,end.fX,end.fY);}
 CHECK(idle(&app,120) && capture(&app,picture) && key(&app,XUI_KEY_ESCAPE,0) && !xideSignatureViewIsOpen(&app));
 CHECK(!strcmp(xuiCodeEditGetText(d->editor),body));
 CHECK(xideSignatureViewAction(&app,XIDE_ACTION_SIGNATURE) && waitHint(&app));
 CHECK(position(&app,d,0) && idle(&app,120) && !xideSignatureViewIsOpen(&app));
 /* Ordinary typing starts/retriggers the same parameter window. */
 const char* plain="func plus(int left,string right=\"\") as int\n return left\nend\nfunc main(...) as int\n return plus\nend\n";
 CHECK(prepare(&app,d,plain,"return plus",11) && character(&app,'(') && waitHint(&app));
 CHECK(s->result->items[s->selected].activeParameter==0 && character(&app,'7') && character(&app,',') && waitHint(&app));
 CHECK(s->result->items[s->selected].activeParameter==1 && character(&app,'\"') && character(&app,0x4e2d) && waitHint(&app));
 CHECK(s->result->items[s->selected].activeParameter==1 && key(&app,XUI_KEY_BACKSPACE,0) && waitHint(&app));
 CHECK(key(&app,XUI_KEY_ESCAPE,0) && !s->wanted && !xideSignatureViewIsOpen(&app));
 /* Real IME preedit withdraws the view, then resumes after commit. */
 CHECK(prepare(&app,d,body,"plus(7,",8) && xideSignatureViewAction(&app,XIDE_ACTION_SIGNATURE) && waitHint(&app));
 CHECK(xuiInputImeComposition(app.context,"中",3,0,3)==XUI_OK && settle(&app) && !xideSignatureViewIsOpen(&app));
 xui_ime_composition_t committed={0};committed.iSize=sizeof(committed);committed.sText="中";committed.iTextSize=3;
 CHECK(xuiInputImeCompositionEx(app.context,&committed)==XUI_OK && settle(&app) && waitHint(&app) && s->result->items[s->selected].activeParameter==1);
 /* Independent completion keeps the hint and the one transport/session. */
 const char* mixed="func plus(int left,int right) as int\n return left\nend\nfunc main(...) as int\n int completionLocal=7\n return plus(7,completionL)\nend\n";
 CHECK(prepare(&app,d,mixed,"completionL)",11) && xideSignatureViewAction(&app,XIDE_ACTION_SIGNATURE) && waitHint(&app));
 xide_language_client* client=app.completionView->client;uint64_t session=app.completionView->session;int signatureRequest=s->request;
 CHECK(key(&app,XUI_KEY_SPACE,XUI_MOD_CTRL));
 uint64_t until=GetTickCount64()+10000;
 while(!app.completionView->ready && GetTickCount64()<until){CHECK(settle(&app));Sleep(2);}
 printf("Concurrent completion wanted=%d pending=%d ready=%d request=%d count=%d open=%d\n",app.completionView->wanted,app.completionView->pending,app.completionView->ready,app.completionView->request,app.completionView->count,xuiCodeEditIsCompletionOpen(d->editor));
 CHECK(app.completionView->ready && xuiCodeEditIsCompletionOpen(d->editor) && xideSignatureViewIsOpen(&app));
 CHECK(app.completionView->client==client && app.completionView->session==session && s->request==signatureRequest && app.completionView->request!=signatureRequest);
 CHECK(key(&app,XUI_KEY_ESCAPE,0) && !xideSignatureViewIsOpen(&app));
 /* Native overload preference and pointer-operated switching preserve text. */
 const char* overload="func choose(int left,int right) as int\n return left\nend\nfunc choose(string left,int right) as int\n return right\nend\nfunc main(...) as int\n return choose(\"x\",7)\nend\n";
 CHECK(prepare(&app,d,overload,"choose(\"x\",",11) && xideSignatureViewAction(&app,XIDE_ACTION_SIGNATURE) && waitHint(&app));
 CHECK(s->result->count==2 && strstr(s->result->items[s->selected].label,"string left"));size_t selectedOverload=s->selected;
 CHECK(click(&app,s->next) && s->selected!=selectedOverload && xuiGetFocusWidget(app.context)==d->editor && !strcmp(xuiCodeEditGetText(d->editor),overload));
 CHECK(xideWorkbenchApplyTheme(&app,IDE_THEME_GRAPHITE)==XUI_OK && idle(&app,80));
 char dark[32768];snprintf(dark,sizeof(dark),"%s-dark.png",picture);CHECK(capture(&app,dark));
 /* This is a movable/resizable native XUI window, with no modal backdrop. */
 xui_rect_t box=xuiWidgetGetRect(s->window),title=xuiWindowGetTitleBarRect(s->window);float x=box.fX+90,y=box.fY+title.fY+title.fH/2;
 CHECK(xuiInputPointerDown(app.context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && settle(&app));
 CHECK(xuiInputPointerMove(app.context,x+40,y+22,XUI_POINTER_BUTTON_LEFT)==XUI_OK && settle(&app));
 CHECK(xuiInputPointerUp(app.context,x+40,y+22,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(&app));
 xui_rect_t moved=xuiWidgetGetRect(s->window);CHECK(abs(moved.fX-box.fX)>15 && abs(moved.fY-box.fY)>10 && xideSignatureViewIsOpen(&app));
 x=moved.fX+moved.fW-2;y=moved.fY+moved.fH-2;
 printf("Resize edges=%u at=%u resizable=%d world=%d/%d/%d/%d local=%d/%d/%d/%d\n",xuiWindowGetResizeEdges(s->window),xuiWindowGetResizeEdgesAt(s->window,x,y),xuiWindowIsResizable(s->window),xuiWidgetGetWorldRect(s->window).fX,xuiWidgetGetWorldRect(s->window).fY,xuiWidgetGetWorldRect(s->window).fW,xuiWidgetGetWorldRect(s->window).fH,moved.fX,moved.fY,moved.fW,moved.fH);
 CHECK(xuiInputPointerDown(app.context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && settle(&app));
 CHECK(xuiInputPointerMove(app.context,x-100,y-30,XUI_POINTER_BUTTON_LEFT)==XUI_OK && settle(&app));
 CHECK(xuiInputPointerUp(app.context,x-100,y-30,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(&app));
 printf("Resize before=%d/%d after=%d/%d\n",moved.fW,moved.fH,xuiWidgetGetRect(s->window).fW,xuiWidgetGetRect(s->window).fH);
 CHECK(xuiWidgetGetRect(s->window).fW<moved.fW-40 && !xideActionsHasModal(&app));
 CHECK(click(&app,xuiWindowGetCloseButtonWidget(s->window)) && !s->wanted && !xideSignatureViewIsOpen(&app));
 CHECK(prepare(&app,d,body,"plus(7,",7) && xideSignatureViewAction(&app,XIDE_ACTION_SIGNATURE) && waitHint(&app));
 CHECK(xuiSetFocusWidget(app.context,app.workspace)==XUI_OK && idle(&app,60) && !s->wanted && !xideSignatureViewIsOpen(&app));
 /* A modal business window withdraws hints and keeps keyboard ownership. */
 CHECK(position(&app,d,(int)(strstr(body,"plus(7,")-body)+7) && xideSignatureViewAction(&app,XIDE_ACTION_SIGNATURE) && waitHint(&app));
 CHECK(xideActionExecute(&app,XIDE_ACTION_NEW_PROJECT)==XIDE_ACTION_DISPATCHED && idle(&app,60));
 CHECK(!s->wanted && !xideSignatureViewIsOpen(&app) && xideActionsHasModal(&app) && !xideActionQuery(&app,XIDE_ACTION_SIGNATURE).enabled);
 CHECK(click(&app,xuiWindowGetCloseButtonWidget(xideTemplateViewWindow(&app))));
 CHECK(position(&app,d,(int)(strstr(body,"plus(7,")-body)+7) && xideSignatureViewAction(&app,XIDE_ACTION_SIGNATURE) && waitHint(&app));
 /* Explicit configuration replacement retires the connection and its hint. */
 session=app.completionView->session;xideCompletionViewConfigurationChanged(&app);
 CHECK(!s->wanted && !xideSignatureViewIsOpen(&app) && !app.completionView->client && app.completionView->session>session);
 CHECK(xideSignatureViewAction(&app,XIDE_ACTION_SIGNATURE) && waitHint(&app) && app.completionView->initialized);
 /* A changed unsaved dependency invalidates the shared sent-input identity. */
 char* dependency=xidePathResolve(f->directory,"dependency.xl");const char* depBody="func helper() as int\n return 1\nend\n";
 CHECK(dependency && xrtFileWriteAtomic(dependency,(xbytesview){(const unsigned char*)depBody,strlen(depBody)}) && xideEditorOpen(&app,dependency,&error));
 xide_document* dep=app.activeDocument;free(dependency);
 CHECK(position(&app,d,(int)(strstr(body,"plus(7,")-body)+7) && xideSignatureViewAction(&app,XIDE_ACTION_SIGNATURE) && waitHint(&app));
 signatureRequest=s->request;uint64_t epoch=app.completionView->inputEpoch;
 CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(dep->editor),0,0,"// unsaved change\n")==XUI_OK && settle(&app) && !xideSignatureViewIsOpen(&app));
 CHECK(waitHint(&app) && s->request!=signatureRequest && s->epoch>epoch && s->epoch==app.completionView->inputEpoch && s->result->items[s->selected].activeParameter==1);
 /* A new editor identity and a C language never inherit the old hint. */
 CHECK(position(&app,d,(int)(strstr(body,"plus(7,")-body)+7) && xideSignatureViewAction(&app,XIDE_ACTION_SIGNATURE) && waitHint(&app));
 uint64_t id=d->id;CHECK(xideEditorClose(&app,d,1,&error) && settle(&app));xide_document* reused=xideDocumentNew(&app.documents,"c",&error);
 CHECK(reused==d && reused->id!=id && xideEditorShow(&app,reused)==XUI_OK && idle(&app,60) && !s->wanted && !xideActionQuery(&app,XIDE_ACTION_SIGNATURE).enabled);
 xide_text_file original={0};CHECK(xideTextFileRead(entry,&original,&error) && !strcmp(original.text,saved));xideTextFileClear(&original);
 printf("signature_view_test passed: %d checks; actual shared native client/XUIwindow,manual/automatic/IME,active parameter,shared completion,overloads,drag/resize/themes,focus/Esc/source identity guards;render=%s\n",f->checks,picture);
cleanup:
 if(began)xgeEnd();
 xideAppDestroy(&app);free(entry);free(picture);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* value){fixture* f=value;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1500;d.iHeight=950;d.sTitle="xlang parameter window verification";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;int r=xgeInit(&d);if(r==XGE_OK)r=xgeRun(verify,f);xgeUnit();return r==XGE_OK && f->started && !f->failed ? 0 : 1;}
int main(int argc,char** argv){if(argc<3 || argc>4 || (argc==4 && strcmp(argv[3],"legacy")) || !xrtDirCreateAll(argv[1]))return 2;fixture f={.directory=argv[1],.compiler=argv[2],.legacy=argc==4};wchar_t name[100];swprintf(name,100,L"XlangSignatureVerification-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(t){if(WaitForSingleObject(t,120000)!=WAIT_OBJECT_0){fputs("signature_view_test timed out\n",stderr);ExitProcess(1);}GetExitCodeThread(t,&code);CloseHandle(t);}CloseDesktop(f.desktop);return (int)code;}
