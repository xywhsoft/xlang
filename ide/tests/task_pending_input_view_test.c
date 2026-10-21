#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/workspace_view.h"
#include "platform/path.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(e) do { ++f->checks; if(!(e)) { fprintf(stderr,"task_pending_input_view_test:%d: %s; %s; %s\n",__LINE__,#e,error.message,info.error); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int sameFile(const char* path,const char* text) { size_t n=0; unsigned char* bytes=xrtFileReadAllLimit(path,4096,&n); int same=bytes && n==strlen(text) && !memcmp(bytes,text,n); xrtFree(bytes); return same; }
static int settle(xide_app* app) { if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0; xideTaskViewPoll(app); xideEditorPoll(app); xideWorkbenchLayout(app); return xuiLayout(app->context)==XUI_OK && !app->frameError; }
static int waitTask(xide_app* app,xide_task* task,xide_task_info* info) { ULONGLONG start=GetTickCount64(); do { if(!settle(app)) return 0; xideTaskInfo(task,info); if(info->phase==XIDE_TASK_DONE && xideTaskViewSettled(app)) return xuiTerminalFlush(app->panels[XIDE_TERMINAL])==XUI_OK; Sleep(5); } while(GetTickCount64()-start<45000); return 0; }
static int key(xide_app* app,int value) { return xuiInputKeyDownEx(app->context,value,0,NULL)==XUI_OK && xuiInputKeyUpEx(app->context,value,0,NULL)==XUI_OK && settle(app); }
static int capture(xide_app* app,const char* path) { xui_rect_i_t rect={0,0,app->viewportWidth,app->viewportHeight}; return xuiUpdate(app->context,0)==XUI_OK && app->proxy.surfaceClear(&app->proxy,app->target,0xffffffffu)==XUI_OK && xuiRender(app->context,app->target,&rect,1)==XUI_OK && xideAppCapture(app,path)==XGE_OK; }
static wchar_t* wide(const char* text) { int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,NULL,0); wchar_t* p=n ? malloc((size_t)n*sizeof(*p)) : NULL; if(p) MultiByteToWideChar(CP_UTF8,0,text,-1,p,n); return p; }
static int notice(xide_task* task,const char* expected) { xide_diagnostic d={0}; if(xideTaskDiagnosticCount(task)!=1 || !xideTaskDiagnostic(task,0,&d)) return 0; int okay=d.kind==XIDE_DIAGNOSTIC_UNSAVED_INPUT && d.line==0 && d.column==0 && xidePathEqual(d.path,expected) && strstr(d.message,"提交时"); xideDiagnosticClear(&d); return okay; }

static int verify(void* user)
{
    fixture* f=user; if(f->started) return XGE_OK; f->started=1;
    xide_app app={0}; xide_model_error error={0}; xide_task_info info={0}; int began=0;
    char* owned[20]={0}; size_t used=0; unsigned char* previous=NULL; size_t previousSize=0; HANDLE locked=INVALID_HANDLE_VALUE; wchar_t *headerWide=NULL,*aliasWide=NULL;
    #define KEEP(p) (owned[used++]=(p))
    char* id=xideCreateId("pending"); char* root=KEEP(id ? xidePathResolve(f->directory,id) : NULL); free(id); CHECK(root && xrtDirCreateAll(root));
    char *projectRoot=KEEP(xidePathResolve(root,"项目")),*header=KEEP(xidePathResolve(root,"外部 文档😀.h")),*alias=KEEP(xidePathResolve(root,"头文件别名.h")),*unrelated=KEEP(xidePathResolve(root,"无关 文档.c"));
    char *entry=KEEP(xidePathResolve(projectRoot,"main.xl")),*native=KEEP(xidePathResolve(projectRoot,"native.c")),*manifest=KEEP(xidePathResolve(projectRoot,"工程.xlp")),*settings=KEEP(xidePathResolve(root,"settings.json")),*output=KEEP(xidePathResolve(projectRoot,"out/app.exe"));
    char *light=KEEP(xidePathResolve(root,"native-pending-input-light.png")),*dark=KEEP(xidePathResolve(root,"native-pending-input-dark.png"));
    CHECK(projectRoot && header && alias && unrelated && entry && native && manifest && settings && output && light && dark && xrtDirCreateAll(projectRoot));
    const char *saved="#pragma once\n// 外部 中文😀\nstatic int header_value(void) { return 41; }\n",*draft="#pragma once\n// 外部 中文😀\nstatic int header_value(void) { return 42; }\n";
    const char* xl="extern \"native_answer\" func native_answer() as c_int\nfunc main(...) as int\n c_int answer = native_answer()\n if answer != 42 then\n  return 1\n end\n print(\"PENDING_HEADER_42\")\n return 0\nend\n";
    const char* c="#include \"../外部 文档😀.h\"\nint native_answer(void) { return header_value(); }\n";
    const char* json="{\"format\":\"xlang-project-1\",\"kind\":\"exe\",\"entry\":\"main.xl\",\"cSources\":[\"native.c\"],\"ide\":{\"format\":\"xlang-ide-project-1\",\"name\":\"实际未保存依赖\",\"configurations\":[{\"id\":\"dev\",\"name\":\"开发\",\"output\":\"out/app.exe\"}]}}";
    CHECK(writeText(header,saved) && writeText(unrelated,"int unrelated = 1;\n") && writeText(entry,xl) && writeText(native,c) && writeText(manifest,json));
    app.hidden=1; app.noSession=1; app.settingsPath=settings; app.compilerPath=f->compiler;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; CHECK(settle(&app));
    CHECK(xideWorkspaceOpen(&app,manifest,1,&error) && settle(&app) && app.workspaceModel.projectCount==1);
    xideDispatchAction(&app,XIDE_ACTION_BUILD); xide_task* baseline=xideTaskViewSelected(&app);
    CHECK(baseline && waitTask(&app,baseline,&info) && info.state==XIDE_TASK_SUCCEEDED && info.built && info.inputsVerified && !info.ran);
    previous=xrtFileReadAllLimit(output,16u*1024u*1024u,&previousSize); CHECK(previous && previousSize>0);
    CHECK(xideEditorOpen(&app,header,&error) && settle(&app)); xide_document* h=app.activeDocument;
    CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(h->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(h->editor)),draft)==XUI_OK && settle(&app) && h->dirty && xuiCodeSelectionSetRange(xuiCodeEditGetSelection(h->editor),xuiCodeEditGetDocument(h->editor),2,7)==XUI_OK);
    uint64_t documentId=h->id; uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(h->editor));
    CHECK(xideEditorOpen(&app,unrelated,&error) && settle(&app)); xide_document* other=app.activeDocument;
    CHECK(xuiCodeEditSetText(other->editor,"int unrelated = 2;\n")==XUI_OK && settle(&app) && other->dirty);
    xideDispatchAction(&app,XIDE_ACTION_RUN); xide_task* blocked=xideTaskViewSelected(&app);
    CHECK(blocked && blocked!=baseline && waitTask(&app,blocked,&info) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.built && !info.ran && notice(blocked,header));
    size_t currentSize=0; unsigned char* current=xrtFileReadAllLimit(output,16u*1024u*1024u,&currentSize); int unchanged=current && currentSize==previousSize && !memcmp(current,previous,previousSize); xrtFree(current); CHECK(unchanged && sameFile(header,saved) && sameFile(unrelated,"int unrelated = 1;\n"));
    xui_dock_window_info_t wi={0}; wi.iSize=sizeof(wi); CHECK(xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_PROBLEMS],&wi)==XUI_OK && xuiDockPanelGetPaneActiveWindow(app.dock,wi.iPane)==app.windows[XIDE_PROBLEMS]);
    const char* presentation[]={"仅测试问题页归属的界面通知"};
    CHECK(xideTaskViewShowLanguageProblemsRouted(&app,"后台界面更新",presentation,1,0,f,NULL)==XUI_OK && xuiTreeViewGetNodeCount(app.panels[XIDE_PROBLEMS])==1);
    CHECK(xideTaskViewShowLanguageProblemsRouted(&app,"显式检查界面",presentation,1,1,f,NULL)==XUI_OK && xideTaskViewLanguageProblemsOwner(&app)==f && xuiTreeViewGetNodeCount(app.panels[XIDE_PROBLEMS])==2);
    CHECK(xideTaskViewSelect(&app,blocked)==XUI_OK && settle(&app) && !xideTaskViewLanguageProblemsOwner(&app) && xuiTreeViewGetNodeCount(app.panels[XIDE_PROBLEMS])==1);
    CHECK(xuiTreeViewGetNodeCount(app.panels[XIDE_PROBLEMS])==1 && xuiTreeViewSetSelected(app.panels[XIDE_PROBLEMS],1)==XUI_OK && xuiSetFocusWidget(app.context,app.panels[XIDE_PROBLEMS])==XUI_OK && settle(&app) && key(&app,XUI_KEY_ENTER));
    int start=0,end=0; CHECK(app.activeDocument==h && h->id==documentId && h->dirty && other->dirty && xuiCodeSelectionGetRange(xuiCodeEditGetSelection(h->editor),&start,&end)==XUI_OK && start==2 && end==7 && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(h->editor))==version);
    CHECK(capture(&app,light) && xideWorkbenchApplyTheme(&app,IDE_THEME_GRAPHITE)==XUI_OK && settle(&app) && capture(&app,dark));
    headerWide=wide(header); aliasWide=wide(alias); CHECK(headerWide && aliasWide);
    locked=CreateFileW(headerWide,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); CHECK(locked!=INVALID_HANDLE_VALUE);
    CHECK(!xideDocumentSave(h,NULL,0,&error) && h->dirty && other->dirty && sameFile(header,saved));
    CHECK(CloseHandle(locked)); locked=INVALID_HANDLE_VALUE; memset(&error,0,sizeof(error));
    xideDispatchAction(&app,XIDE_ACTION_SAVE_FILE); CHECK(settle(&app) && !h->dirty && other->dirty && sameFile(header,draft) && xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(h->editor)));
    xideDispatchAction(&app,XIDE_ACTION_RUN); xide_task* success=xideTaskViewSelected(&app);
    CHECK(success && success!=blocked && waitTask(&app,success,&info) && info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && info.exit.Code==0 && xideTaskDiagnosticCount(success)==0 && other->dirty);
    char terminal[8192]={0}; CHECK(xuiTerminalSerializeText(app.panels[XIDE_TERMINAL],terminal,sizeof(terminal))>=0 && strstr(terminal,"PENDING_HEADER_42"));
    CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(h->editor))==XUI_OK && settle(&app) && h->dirty && !strcmp(xuiCodeEditGetText(h->editor),saved) && CreateHardLinkW(aliasWide,headerWide,NULL));
    char* paths[]={alias,header,unrelated}; xide_strings pending={.items=paths,.count=3,.capacity=3};
    xide_task* aliased=xideTasksBuildGuarded(app.tasks,&app.workspaceModel.projects[0],"dev",f->compiler,1,&pending,&error); CHECK(aliased && xideTaskViewSelect(&app,aliased)==XUI_OK && waitTask(&app,aliased,&info) && info.invalidated && !info.ran && notice(aliased,alias));
    CHECK(xideTaskViewJump(&app,0) && app.activeDocument==h && h->id==documentId && h->dirty && other->dirty);
    CHECK(sameFile(unrelated,"int unrelated = 1;\n") && sameFile(entry,xl) && sameFile(native,c) && app.workspaceModel.projectCount==1 && app.workspaceModel.projects[0].files.count==2);
    printf("task_pending_input_view_test passed: %d native assertions; actual compiler-discovered external C header, one typed location-free input notice, real Enter opens existing dirty document preserving selection/version/undo, unrelated dirty document excluded, old output preserved and never run, failed save stays dirty, explicit save plus real mixed build/run, physical alias deduplication; two native images\n",f->checks);
cleanup:
    if(locked!=INVALID_HANDLE_VALUE) { CloseHandle(locked); }
    if(began) { xgeEnd(); }
    xideAppDestroy(&app); xrtFree(previous); free(headerWide); free(aliasWide); for(size_t i=0;i<used;++i) free(owned[i]); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* user) { fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=900; d.sTitle="xlang IDE actual pending build input verification"; d.iFlags=XGE_INIT_WINDOW; d.iRunMode=XGE_RUN_GAME_LOOP; int ret=xgeInit(&d); if(ret==XGE_OK) ret=xgeRun(verify,f); xgeUnit(); return ret==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=3 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argv[2]}; wchar_t name[100]; swprintf(name,100,L"XlangPendingInputVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(t) { if(WaitForSingleObject(t,180000)!=WAIT_OBJECT_0) { fputs("task_pending_input_view_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); } CloseDesktop(f.desktop); return (int)code; }
