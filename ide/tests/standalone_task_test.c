#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/workspace_view.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(e) do { ++f->checks; if(!(e)) { fprintf(stderr,"standalone_task_test:%d: %s; %s; %s; state=%d phase=%d built=%d ran=%d receipt=%d invalid=%d exit=%d\n",__LINE__,#e,error.message,info.error,info.state,info.phase,info.built,info.ran,info.inputsVerified,info.invalidated,info.exit.Code); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideTaskViewPoll(a); xideEditorPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int waitTask(xide_app* a,xide_task* t,xide_task_info* info) { ULONGLONG until=GetTickCount64()+45000; do { if(!settle(a)) return 0; xideTaskInfo(t,info); if(info->phase==XIDE_TASK_DONE && xideTaskViewSettled(a)) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
static int key(xide_app* a,int code) { return (!a->activeDocument || xideEditorShow(a,a->activeDocument)==XUI_OK) && xuiInputKeyDownEx(a->context,code,0,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,code,0,NULL)==XUI_OK && settle(a); }
static int capture(xide_app* a,const char* path) { xui_rect_i_t rect={0,0,a->viewportWidth,a->viewportHeight}; return xuiUpdate(a->context,0)==XUI_OK && a->proxy.surfaceClear(&a->proxy,a->target,0xffffffffu)==XUI_OK && xuiRender(a->context,a->target,&rect,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK; }
static int invocationHas(xide_task* task,const char* source)
{
    char* directory=xidePathDirectory(xideTaskBuildLog(task)); char* path=directory ? xidePathResolve(directory,"build-invocation.json") : NULL;
    size_t size=0; unsigned char* raw=path ? xideIoReadAllLimit(path,1024*1024,&size) : NULL;
    xvalue* root=raw ? xrtJsonParse((xstrview){(char*)raw,size}) : NULL; xstrview actual={0};
    int okay=root && xrtValueGetString(xrtValueArrayGet(xrtValueObjectGet(root,(xstrview){"arguments",9}),1),&actual) && actual.Size==strlen(source) && !memcmp(actual.Data,source,actual.Size);
    xrtValueRelease(root); xrtFree(raw); free(directory); free(path); return okay;
}
static int verify(void* user)
{
    fixture* f=user; f->started=1; xide_app app={0}; xide_model_error error={0}; xide_task_info info={0};
    xide_project project={0}; xide_workspace workspace={0}; int began=0; char* owned[32]={0}; size_t used=0;
    unsigned char *oldOutput=NULL,*actual=NULL; size_t oldSize=0,actualSize=0; char* cOutput=NULL;
    #define KEEP(p) (owned[used++]=(p))
    char* id=xideCreateId("standalone"); char* root=KEEP(id ? xidePathResolve(f->directory,id) : NULL); free(id);
    CHECK(root && xrtDirCreateAll(root));
    char *c=KEEP(xidePathResolve(root,"独立 C.c")),*xl=KEEP(xidePathResolve(root,"独立 xlang.xl")),*header=KEEP(xidePathResolve(root,"value.h")),*dep=KEEP(xidePathResolve(root,"dep.xl"));
    char *settings=KEEP(xidePathResolve(root,"settings.json")),*relative=KEEP(xidePathResolve(root,"relative.txt")),*xlp=KEEP(xidePathResolve(root,"工程.xlp")),*beta=KEEP(xidePathResolve(root,"其他.xlp")),*xlw=KEEP(xidePathResolve(root,"工作空间.xlw"));
    char *cImage=KEEP(xidePathResolve(root,"single-c.png")),*xlImage=KEEP(xidePathResolve(root,"single-xlang.png"));
    CHECK(c && xl && header && dep && settings && relative && xlp && beta && xlw && cImage && xlImage);
    const char* cText="#include <stdio.h>\n#include \"value.h\"\nint main(void) { FILE* f=fopen(\"relative.txt\",\"rb\"); if(!f) return 1; int v=fgetc(f); fclose(f); puts(\"C_SINGLE_42\"); return v!='O' || VALUE!=42; }\n";
    const char* xlText="import dep\nfunc main(...) as int\n if dep.value() != 42 then\n  return 1\n end\n print(\"XL_SINGLE_42\")\n return 0\nend\n";
    CHECK(writeText(c,cText) && writeText(xl,xlText) && writeText(header,"#define VALUE 42\n") && writeText(dep,"export func value() as int\n return 42\nend\n") && writeText(relative,"OK"));
    app.hidden=1; app.noSession=1; app.compilerPath=f->compiler; app.settingsPath=settings;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
    CHECK(!app.workspaceModel.projectCount && !app.workspaceModel.manifest && xideEditorOpen(&app,c,&error));
    xide_document* cDoc=app.activeDocument;
    CHECK(xideActionQuery(&app,XIDE_ACTION_BUILD).enabled && xideActionQuery(&app,XIDE_ACTION_RUN).enabled && key(&app,XUI_KEY_F7));
    xide_task* task=xideTaskViewSelected(&app); CHECK(task && waitTask(&app,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.built && !info.ran && info.inputsVerified && invocationHas(task,c));
    CHECK(!xidePathExists(xlp) && !xidePathExists(xlw) && !app.workspaceModel.source && key(&app,XUI_KEY_F5));
    task=xideTaskViewSelected(&app); CHECK(task && waitTask(&app,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.ran && info.exit.Code==0 && capture(&app,cImage));
    cOutput=KEEP(xidePathResolve(NULL,xideTaskArtifact(task))); CHECK(cOutput); oldOutput=xrtFileReadAllLimit(cOutput,4*1024*1024,&oldSize); CHECK(oldOutput && oldSize);
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(cDoc->editor),0,"// saved automatically\n")==XUI_OK && cDoc->dirty && key(&app,XUI_KEY_F7));
    task=xideTaskViewSelected(&app); CHECK(task && waitTask(&app,task,&info) && info.state==XIDE_TASK_SUCCEEDED && !cDoc->dirty);
    xrtFree(oldOutput); oldOutput=xrtFileReadAllLimit(cOutput,4*1024*1024,&oldSize); CHECK(oldOutput);
    CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(cDoc->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(cDoc->editor)),"int main( { broken\n")==XUI_OK && key(&app,XUI_KEY_F5));
    task=xideTaskViewSelected(&app); CHECK(task && waitTask(&app,task,&info) && info.state==XIDE_TASK_FAILED && !info.built && !info.ran && xideTaskDiagnosticCount(task));
    actual=xrtFileReadAllLimit(cOutput,4*1024*1024,&actualSize); CHECK(actual && actualSize==oldSize && !memcmp(actual,oldOutput,oldSize)); xrtFree(actual); actual=NULL;
    CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(cDoc->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(cDoc->editor)),cText)==XUI_OK && xideDocumentSave(cDoc,NULL,0,&error));
    CHECK(xideEditorOpen(&app,header,&error)); xide_document* h=app.activeDocument;
    CHECK(!xideActionQuery(&app,XIDE_ACTION_BUILD).enabled && !xideActionQuery(&app,XIDE_ACTION_RUN_FILE).enabled);
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(h->editor),0,"// unsaved dependency\n")==XUI_OK && xideEditorShow(&app,cDoc)==XUI_OK && key(&app,XUI_KEY_F5));
    task=xideTaskViewSelected(&app); CHECK(task && waitTask(&app,task,&info) && info.invalidated && !info.ran && h->dirty);
    CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(h->editor))==XUI_OK && !h->dirty && xideEditorOpen(&app,xl,&error) && key(&app,XUI_KEY_F5));
    xide_document* xlDoc=app.activeDocument; task=xideTaskViewSelected(&app);
    CHECK(task && waitTask(&app,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && info.inputsVerified);
    CHECK(invocationHas(task,xl) && strcmp(xideTaskArtifact(task),cOutput) && capture(&app,xlImage));
    CHECK(!app.workspaceModel.projectCount && !xidePathExists(xlp) && !xidePathExists(xlw));
    xide_document* unnamed=xideDocumentNew(&app.documents,"xlang",&error); CHECK(unnamed && xideEditorShow(&app,unnamed)==XUI_OK && xideActionQuery(&app,XIDE_ACTION_RUN).enabled);
    size_t tasks=xideTasksCount(app.tasks); CHECK(xideActionExecute(&app,XIDE_ACTION_RUN)==XIDE_ACTION_DISPATCHED && xideTasksCount(app.tasks)==tasks && !unnamed->path);
    CHECK(xideEditorClose(&app,unnamed,1,&error) && xideEditorShow(&app,xlDoc)==XUI_OK);
    CHECK(xideProjectCreate(&project,xlp,"独立工程",xl,&error) && xideProjectSave(&project,NULL,&error)); xideProjectClear(&project);
    CHECK(xideWorkspaceOpen(&app,xlp,1,&error) && !app.workspaceModel.manifest && app.projectSessionPath && app.workspaceModel.projectCount==1 && key(&app,XUI_KEY_F5));
    task=xideTaskViewSelected(&app); CHECK(task && waitTask(&app,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.ran && !xidePathExists(xlw));
    CHECK(xideProjectCreate(&project,beta,"所选工程",xl,&error) && xideProjectSave(&project,NULL,&error)); xideProjectClear(&project);
    CHECK(xideWorkspaceCreate(&workspace,"四种目标",&error) && xideWorkspaceAddProject(&workspace,xlp,"startup",&error) && xideWorkspaceAddProject(&workspace,beta,"selected",&error) && xideWorkspaceSetStartup(&workspace,"startup",&error) && xideWorkspaceSave(&workspace,xlw,&error));
    CHECK(xideWorkspaceOpen(&app,xlw,0,&error) && xideWorkspaceViewSelectProject(&app,"selected") && key(&app,XUI_KEY_F7));
    task=xideTaskViewSelected(&app); CHECK(task && waitTask(&app,task,&info) && info.state==XIDE_TASK_SUCCEEDED && !info.ran && !strcmp(xideTaskProjectId(task),"selected"));
    CHECK(key(&app,XUI_KEY_F5)); task=xideTaskViewSelected(&app); CHECK(task && waitTask(&app,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.ran && !strcmp(xideTaskProjectId(task),"startup"));
    CHECK(xideEditorOpen(&app,c,&error) && xideActionExecute(&app,XIDE_ACTION_RUN_FILE)==XIDE_ACTION_DISPATCHED); task=xideTaskViewSelected(&app);
    CHECK(task && waitTask(&app,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.ran && invocationHas(task,c) && !strcmp(app.selectedProject,"selected") && !strcmp(app.workspaceModel.startupProject,"startup"));
    printf("standalone_task_test passed: %d checks; real C/xlang file builds and runs without manifests, source CWD/header/imports/receipts, per-file output isolation, save before build, failure preserves old executable, unsaved dependency block, untitled cancellation, independent xlp and workspace selected/startup targets, explicit file override, two native images.\n",f->checks);
cleanup:
    xrtFree(oldOutput); xrtFree(actual); xideProjectClear(&project); xideWorkspaceClear(&workspace);
    if(began) xgeEnd();
    xideAppDestroy(&app); for(size_t i=0;i<used;++i) free(owned[i]); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* user) { fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1280; d.iHeight=800; d.sTitle="Standalone build targets"; d.iFlags=XGE_INIT_WINDOW; d.iRunMode=XGE_RUN_GAME_LOOP; int r=xgeInit(&d); if(r==XGE_OK) r=xgeRun(verify,f); xgeUnit(); return r==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv) { if(argc!=3 || !xrtDirCreateAll(argv[1])) return 2; fixture f={.directory=argv[1],.compiler=argv[2]}; wchar_t name[96]; swprintf(name,96,L"StandaloneTargets-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1; HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(t) { if(WaitForSingleObject(t,180000)!=WAIT_OBJECT_0) { fputs("standalone_task_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); } CloseDesktop(f.desktop); return (int)code; }
