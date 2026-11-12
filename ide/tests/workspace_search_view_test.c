#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/dialog.h"
#include "ui/workspace_view.h"
#include "services/file_sync.h"
#include "platform/path.h"
#include "catalog_ready.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Actual view for deterministic late-result acceptance and shared-sync checks. */
#include "../src/ui/language_view.c"
typedef struct fixture {const char *directory,*compiler;HDESK desktop;int started,failed;} fixture;
#define CHECK(v) do{if(!(v)){fprintf(stderr,"workspace_search_view_test:%d:%s;%s;%s\n",__LINE__,#v,error.message,app.languageView && app.languageView->workspaceSearch?app.languageView->workspaceSearch->message:"");f->failed=1;goto cleanup;}}while(0)
static int writeText(const char* path,const char* body){return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)});}
static int settle(xide_app* app){if(xuiDispatchPendingEvents(app->context)!=XUI_OK)return 0;xideEditorPoll(app);xideFileSyncPoll(app);xideTaskViewPoll(app);xideLanguageViewPoll(app);xideWorkbenchLayout(app);return xuiLayout(app->context)==XUI_OK && !app->frameError;}
static int waitRows(xide_app* app,const char* query,size_t count)
{uint64_t until=GetTickCount64()+15000;do{if(!settle(app))return 0;struct xide_workspace_search_view* r=app->languageView->workspaceSearch;if(r->usable && !r->stage && !strcmp(r->query,query) && r->result && r->result->count==count)return 1;Sleep(5);}while(GetTickCount64()<until);return 0;}
static int query(xide_app* app,const char* text,size_t count){return xuiEditSetText(xideLanguageViewWorkspaceSymbolsControl(app,XIDE_WORKSPACE_SYMBOL_QUERY),text)==XUI_OK && waitRows(app,text,count);}
static int waitClosed(xide_app* app){uint64_t until=GetTickCount64()+15000;do{if(!settle(app))return 0;if(!xideLanguageViewWorkspaceSymbolsIsOpen(app))return 1;Sleep(5);}while(GetTickCount64()<until);return 0;}
static int waitFiles(xide_app* app){uint64_t until=GetTickCount64()+10000;do{if(!settle(app))return 0;if(xideFileSyncSettled(app))return 1;Sleep(5);}while(GetTickCount64()<until);return 0;}
static int waitDiskChange(xide_app* app,uint64_t previous){uint64_t until=GetTickCount64()+10000;do{if(!settle(app))return 0;if(xideFileSyncRevision(app)>previous && !app->languageView->syncMode && !automaticWatchPending(app->languageView))return 1;Sleep(2);}while(GetTickCount64()<until);return 0;}
static int waitError(xide_app* app,const char* text){uint64_t until=GetTickCount64()+15000;do{if(!settle(app))return 0;struct xide_workspace_search_view* r=app->languageView->workspaceSearch;if(!r->stage && !r->usable && strstr(r->message,text))return 1;Sleep(5);}while(GetTickCount64()<until);return 0;}
static int key(xide_app* app,int k,uint32_t mods){return xuiInputKeyDown(app->context,k,mods)==XUI_OK && xuiInputKeyUp(app->context,k,mods)==XUI_OK && settle(app);}
static int ime(xide_app* app,const char* text,int active){xui_ime_composition_t c={.iSize=sizeof(c),.sText=text,.iTextSize=(int)strlen(text),.bActive=active};c.iCursor=c.iSelectionStart=c.iSelectionEnd=c.iTextSize;return xuiInputImeCompositionEx(app->context,&c)==XUI_OK && settle(app);}
static int capture(xide_app* app,const char* path){xui_rect_i_t rect={0,0,app->viewportWidth,app->viewportHeight};return settle(app) && xuiUpdate(app->context,0)==XUI_OK && app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&rect,1)==XUI_OK && xideAppCapture(app,path)==XGE_OK;}
static int selected(xide_document* d,const char* body,const char* token)
{xui_code_selection_t s={0};int begin=(int)(strstr(body,token)-body);return xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&s)==XUI_OK && s.iAnchorOffset==begin && s.iCaretOffset==begin+(int)strlen(token);}
static int sendReady(xide_app* app)
{uint64_t until=GetTickCount64()+10000;do{if(!settle(app))return 0;if(app->languageView->workspaceSearch->stage==5)return 1;Sleep(1);}while(GetTickCount64()<until);return 0;}
static xide_workspace_search_result* takeWithoutUi(xide_app* app)
{xide_workspace_search_result* result=NULL;uint64_t until=GetTickCount64()+10000;while(!result && GetTickCount64()<until){result=xideWorkspaceSearchDecoderTake(app->languageView->workspaceSearch->decoder);if(!result)Sleep(1);}return result;}
static int verify(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};xide_project temporary={0};int began=0;xide_workspace_search_result* held=NULL;
 char *root=NULL,*entry=NULL,*helper=NULL,*orphan=NULL,*native=NULL,*projectA=NULL,*projectB=NULL,*workspace=NULL,*light=NULL,*dark=NULL;
 const char* mainBody="import helper\nfunc main(...) as int\n return helper.value()\nend\n";
 const char* helperRaw="\xef\xbb\xbf/* 中文😀 */\r\nclass Payload\r\n int number\r\nend\r\nexport func value() as int\r\n return 1\r\nend\r\n";
 const char* helperBody="/* 中文😀 */\nclass Payload\n int number\nend\nexport func value() as int\n return 1\nend\n";
 const char* helperDirty="/* 中文😀 */\nclass Payload\n int amount\nend\nexport func value() as int\n return 2\nend\n";
 const char* orphanBody="func orphanWork() as int\n return 7\nend\nfunc main(...) as int\n return orphanWork()\nend\n";
 root=xidePathResolve(f->directory,"中文😀 workspace");entry=xidePathResolve(root,"main.xl");helper=xidePathResolve(root,"helper.xl");orphan=xidePathResolve(root,"未打开.xl");native=xidePathResolve(root,"native.c");projectA=xidePathResolve(root,"A.xlp");projectB=xidePathResolve(root,"B.xlp");workspace=xidePathResolve(root,"test.xlw");light=xidePathResolve(root,"workspace-symbols-light.png");dark=xidePathResolve(root,"workspace-symbols-dark.png");
 CHECK(root && entry && helper && orphan && native && projectA && projectB && workspace && light && dark && xrtDirCreateAll(root));CHECK(writeText(entry,mainBody) && writeText(helper,helperRaw) && writeText(orphan,orphanBody) && writeText(native,"int main(void) { return 0; }\n"));
 app.hidden=1;app.noSession=1;app.compilerPath=f->compiler;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(xideTestCatalogReady(&app));
 CHECK(xideWorkspaceCreate(&app.workspaceModel,"符号搜索",&error));
 for(size_t i=0;i<2;++i){CHECK(xideProjectCreate(&temporary,i?projectB:projectA,i?"B":"A",entry,&error) && xideProjectAddFile(&temporary,helper,"xlang",&error) && xideProjectAddFile(&temporary,orphan,"xlang",&error) && xideProjectSave(&temporary,temporary.manifest,&error));xideProjectClear(&temporary);CHECK(xideWorkspaceAddProject(&app.workspaceModel,i?projectB:projectA,i?"project-B":"project-A",&error));}
 CHECK(xideWorkspaceSave(&app.workspaceModel,workspace,&error) && xideWorkspaceViewRefresh(&app)==XUI_OK);xideLanguageViewConfigurationChanged(&app);xideFileSyncRefresh(&app);CHECK(waitFiles(&app));
 CHECK(!app.activeDocument && !app.documents.count && xideActionFind(XIDE_ACTION_WORKSPACE_SYMBOLS)->key=='T' && xideActionFind(XIDE_ACTION_WORKSPACE_SYMBOLS)->modifiers==XUI_MOD_CTRL);
 CHECK(key(&app,'T',XUI_MOD_CTRL) && xideLanguageViewWorkspaceSymbolsIsOpen(&app));CHECK(xuiWidgetGetType(xideLanguageViewWorkspaceSymbolsWindow(&app))==xuiWindowGetType(app.context) && xideActionsHasModal(&app));
 CHECK(query(&app,"Payload number",2) && !app.activeDocument && !app.documents.count && app.languageView->workspaceSearch->result->workerThread!=xrtThreadCurrentId());CHECK(!xrtGetError());CHECK(xidePathLexicalEqual(xuiEditGetText(app.languageView->workspaceSearch->pathPreview),helper) && xuiInputIsReadonly(app.languageView->workspaceSearch->pathPreview));CHECK(capture(&app,light));
 CHECK(xideWorkbenchApplyTheme(&app,1)==XUI_OK && capture(&app,dark));CHECK(xideWorkbenchApplyTheme(&app,0)==XUI_OK);
 xui_widget input=xideLanguageViewWorkspaceSymbolsControl(&app,XIDE_WORKSPACE_SYMBOL_QUERY),tree=xideLanguageViewWorkspaceSymbolsControl(&app,XIDE_WORKSPACE_SYMBOL_RESULTS);
 CHECK(xuiSetFocusWidget(app.context,input)==XUI_OK && key(&app,XUI_KEY_DOWN,0) && xuiGetFocusWidget(app.context)==tree && key(&app,XUI_KEY_ENTER,0) && waitClosed(&app));xide_document* target=app.activeDocument;
 CHECK(target && xidePathLexicalEqual(target->path,helper) && selected(target,helperBody,"number") && target->baseline.bom && target->baseline.eol==XUI_CODE_EOL_CRLF && !target->dirty && target->baseline.rawLength==strlen(helperRaw) && !memcmp(target->baseline.raw,helperRaw,strlen(helperRaw)) && xuiGetFocusWidget(app.context)==target->editor);
 CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(target->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(target->editor)),helperDirty)==XUI_OK && settle(&app));CHECK(xideActionExecute(&app,XIDE_ACTION_WORKSPACE_SYMBOLS)==XIDE_ACTION_DISPATCHED && query(&app,"Payload amount",2));
 CHECK(xuiTreeViewSetSelected(tree,1)==XUI_OK && xuiSetFocusWidget(app.context,tree)==XUI_OK);xui_event_t doubleClick={.iType=XUI_EVENT_POINTER_DOUBLE_CLICK,.iPhase=XUI_EVENT_PHASE_TARGET};CHECK(workspaceDouble(tree,&doubleClick,&app)==XUI_EVENT_DISPATCH_STOP && waitClosed(&app));CHECK(app.activeDocument==target && target->dirty && selected(target,helperDirty,"amount") && xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(target->editor)));
 CHECK(xideActionExecute(&app,XIDE_ACTION_WORKSPACE_SYMBOLS)==XIDE_ACTION_DISPATCHED && query(&app,"orphanWork",2));CHECK(xuiSetFocusWidget(app.context,input)==XUI_OK && ime(&app,"计",1));uint64_t serial=app.languageView->workspaceSearch->serial;CHECK(app.languageView->workspaceSearch->composing && key(&app,XUI_KEY_ENTER,0) && app.languageView->workspaceSearch->serial==serial && xideLanguageViewWorkspaceSymbolsIsOpen(&app));CHECK(ime(&app,"",0));
 CHECK(query(&app,"orphanWork",2) && xideLanguageViewWorkspaceSymbolJump(&app,0) && sendReady(&app));held=takeWithoutUi(&app);CHECK(held && held->valid && held->navigation && held->snapshot.raw && !openPath(&app,orphan));
 uint64_t previousWatch=xideFileSyncRevision(&app),documentToken=xideDocumentsSuspendSources(&app.documents,&error);CHECK(documentToken && xideDocumentsSourcesSuspended(&app.documents,documentToken) && !workspaceApplyNavigation(&app,held));CHECK(xideDocumentsResumeSources(&app.documents,documentToken,&error) && xideFileSyncRevision(&app)==previousWatch && !workspaceApplyNavigation(&app,held) && !openPath(&app,orphan));CHECK(xideWorkspaceSearchDecoderRetire(app.languageView->workspaceSearch->decoder,held));held=NULL;workspaceCancel(app.languageView);CHECK(waitFiles(&app));
 CHECK(query(&app,"orphanWork",2) && xideLanguageViewWorkspaceSymbolJump(&app,0) && sendReady(&app));held=takeWithoutUi(&app);CHECK(held && held->valid && held->navigation && held->snapshot.raw && !openPath(&app,orphan));uint64_t diskRevision=xideFileSyncRevision(&app);
 CHECK(writeText(orphan,"func orphanWork() as int\n return 8\nend\nfunc main(...) as int\n return orphanWork()\nend\n") && !workspaceApplyNavigation(&app,held) && !openPath(&app,orphan));CHECK(xideWorkspaceSearchDecoderRetire(app.languageView->workspaceSearch->decoder,held));held=NULL;workspaceCancel(app.languageView);
 CHECK(waitDiskChange(&app,diskRevision) && !xideLanguageViewWorkspaceSymbolCount(&app));diskRevision=xideFileSyncRevision(&app);CHECK(writeText(orphan,orphanBody) && waitDiskChange(&app,diskRevision));
 CHECK(query(&app,"Payload amount",2) && xideLanguageViewWorkspaceSymbolJump(&app,0) && sendReady(&app));held=takeWithoutUi(&app);CHECK(held && held->valid && !held->snapshot.raw);CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(target->editor),0,0,"\n")==XUI_OK && !workspaceApplyNavigation(&app,held));CHECK(xideWorkspaceSearchDecoderRetire(app.languageView->workspaceSearch->decoder,held));held=NULL;CHECK(settle(&app) && !xideLanguageViewWorkspaceSymbolCount(&app));
 CHECK(xuiSetFocusWidget(app.context,input)==XUI_OK && key(&app,XUI_KEY_ESCAPE,0) && !xideLanguageViewWorkspaceSymbolsIsOpen(&app));CHECK(xideEditorOpen(&app,native,&error) && settle(&app));xide_document* c=app.activeDocument;CHECK(!strcmp(c->language,"c") && key(&app,'T',XUI_MOD_CTRL) && query(&app,"orphanWork",2) && key(&app,XUI_KEY_ESCAPE,0) && app.activeDocument==c);
 CHECK(xideEditorClose(&app,target,1,&error) && xideEditorClose(&app,c,1,&error) && settle(&app));CHECK(xideActionExecute(&app,XIDE_ACTION_WORKSPACE_SYMBOLS)==XIDE_ACTION_DISPATCHED && query(&app,"计值",0));CHECK(xuiSetFocusWidget(app.context,input)==XUI_OK && key(&app,XUI_KEY_ESCAPE,0));
 CHECK(xideProjectAddFile(app.workspaceModel.projects,native,"c",&error));xideLanguageViewConfigurationChanged(&app);CHECK(waitFiles(&app) && xideActionExecute(&app,XIDE_ACTION_WORKSPACE_SYMBOLS)==XIDE_ACTION_DISPATCHED && waitError(&app,"additional frontend support") && !xideLanguageViewWorkspaceSymbolCount(&app));CHECK(xuiSetFocusWidget(app.context,input)==XUI_OK && key(&app,XUI_KEY_ESCAPE,0));
 CHECK(!app.frameError);printf("workspace_search_view_test passed: real zero-open multi-project search,XUI window,Ctrl+T,keyboard/double-click navigation,BOM/CRLF/Unicode prepared baseline,dirty overlay/undo,IME,late disk/edit proof rejection,C-active search and cancellation;renders in %s.\n",root);
cleanup:
 if(held)xideWorkspaceSearchResultRelease(held);
 if(began)xgeEnd();
 xideProjectClear(&temporary);xideAppDestroy(&app);free(root);free(entry);free(helper);free(orphan);free(native);free(projectA);free(projectB);free(workspace);free(light);free(dark);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* p){fixture* f=p;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={.iWidth=1600,.iHeight=1000,.sTitle="Workspace symbols verification",.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI,.iRunMode=XGE_RUN_GAME_LOOP};int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(verify,f);xgeUnit();return result==XGE_OK && f->started && !f->failed?0:1;}
int main(int argc,char** argv){if(argc!=3 || !xrtDirCreateAll(argv[1]))return 2;fixture f={.directory=argv[1],.compiler=argv[2]};wchar_t name[100];swprintf(name,100,L"WorkspaceSymbolVerification-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(t){if(WaitForSingleObject(t,120000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(t,&code);CloseHandle(t);}CloseDesktop(f.desktop);return (int)code;}
