#include "app/xide_app.h"
#include "ui/task_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include "theme/theme.h"
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"task_history_test:%d: %s; %s; task=%s\n",__LINE__,#v,error.message,info.error); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int writeText(const char* p,const char* s) { return xideIoWriteAtomic(p,(xbytesview){(unsigned char*)s,strlen(s)}); }
static int settled(xide_tasks* tasks,xide_task* task,xide_task_info* info)
{
 ULONGLONG end=GetTickCount64()+10000;
 do { xideTasksPoll(tasks); xideTaskInfo(task,info); if(xideTaskInputOpen(task)) xideTaskCloseInput(task); if(info->state>=XIDE_TASK_SUCCEEDED) { xideTasksPoll(tasks); return 1; } Sleep(2); } while(GetTickCount64()<end);
 return 0;
}
static int phase(xide_task* task,enum xide_task_phase wanted,xide_task_info* info)
{
 ULONGLONG end=GetTickCount64()+10000;
 do { xideTaskInfo(task,info); if(info->phase==wanted) return 1; if(info->state>=XIDE_TASK_SUCCEEDED) return 0; Sleep(2); } while(GetTickCount64()<end);
 return 0;
}
static int testConfiguration(xide_project* p,const char* program,const char* mode,const char* argument,xide_model_error* error)
{
 xvalue* configs=xideProjectConfigurationsJson(p); xvalue* test=xrtValueObject(); xvalue* environment=xrtValueObject(); xvalue* args=xrtValueArray();
 int okay=configs && test && environment && args && xrtValueObjectSetNew(environment,sv("XIDE_TASK_MODE"),xrtValueString(sv(mode))) &&
  xrtValueArrayAppendNew(args,xrtValueString(sv(argument))) && xrtValueObjectSetNew(test,sv("program"),xrtValueString(sv(program))) &&
  xrtValueObjectSetNew(test,sv("workingDirectory"),xrtValueString(sv(".")));
 if(okay) { okay=xrtValueObjectSetNew(test,sv("arguments"),args); args=NULL; }
 if(okay) { okay=xrtValueObjectSetNew(test,sv("environment"),environment); environment=NULL; }
 if(okay) { okay=xrtValueObjectSetNew(xrtValueArrayGet(configs,0),sv("test"),test); test=NULL; }
 if(okay) okay=xideProjectApplyConfigurations(p,configs,p->configurations[0].id,error) && xideProjectSave(p,NULL,error);
 xrtValueRelease(configs); xrtValueRelease(test); xrtValueRelease(environment); xrtValueRelease(args); return okay;
}
static uint64_t privateBytes(void)
{
 PROCESS_MEMORY_COUNTERS_EX counters={0}; counters.cb=sizeof(counters);
 return GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&counters,sizeof(counters)) ? counters.PrivateUsage : 0;
}
static int flood(void)
{
 unsigned char bytes[16384]; for(size_t i=0;i<sizeof(bytes);++i) bytes[i]=(unsigned char)('a'+i%26);
 for(int i=0;i<192;++i) { DWORD n=0; if(!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),bytes,sizeof(bytes),&n,NULL) || n!=sizeof(bytes)) return 2; Sleep(1); }
 DWORD n=0; const char end[]="HISTORY_FLOOD_END\n"; return WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),end,sizeof(end)-1,&n,NULL) ? 0 : 3;
}
static int capture(xide_app* a,const char* path)
{
 xideWorkbenchLayout(a); if(xuiLayout(a->context)!=XUI_OK || xuiUpdate(a->context,0)!=XUI_OK) return 0;
 xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK && !a->frameError;
}
static int runChecks(int argc,char** argv)
{
 int failed=0,checks=0,began=0; xide_model_error error={0}; xide_task_info info={0}; xide_project project={0}; xide_tasks* tasks=NULL; xide_app app={0};
 char *root=NULL,*manifest=NULL,*entry=NULL,*native=NULL,*binary=NULL,*program=NULL,*missing=NULL,*logPath=NULL,*cImage=NULL,*xlImage=NULL,*listImage=NULL;
 unsigned char *raw=NULL,*tail=NULL; size_t rawSize=0; uint64_t plateau=0,after=0; xide_task* pending[XIDE_TASK_PENDING_LIMIT]={0};
 CHECK(argc==2); root=xidePathResolve(argv[1],"中文 任务历史"); CHECK(root && xideIoCreateDirectories(root));
 manifest=xidePathResolve(root,"app.xlp"); entry=xidePathResolve(root,"main.xl"); native=xidePathResolve(root,"native.c"); binary=xidePathDirectory(argv[0]);
 program=binary ? xidePathResolve(binary,"xide_task_program.exe") : NULL; missing=xidePathResolve(root,"missing-compiler.exe"); CHECK(manifest && entry && native && program && missing);
 CHECK(writeText(entry,"func main(...) as int\n return 0\nend\n") && writeText(native,"int native(void) { return 42; }\n") && xideProjectCreate(&project,manifest,"任务历史",entry,&error) && xideProjectAddFile(&project,native,"c",&error) && xideProjectSave(&project,NULL,&error));
 tasks=xideTasksCreate(); CHECK(tasks && testConfiguration(&project,program,"wait","0",&error));
 pending[0]=xideTasksTest(tasks,&project,NULL,&error); CHECK(pending[0] && phase(pending[0],XIDE_TASK_TEST,&info));
 for(size_t i=1;i<XIDE_TASK_PENDING_LIMIT;++i) {
  char arg[32]; snprintf(arg,sizeof(arg),"%zu",i); CHECK(testConfiguration(&project,program,"wait",arg,&error)); pending[i]=xideTasksTest(tasks,&project,NULL,&error); CHECK(pending[i]);
 }
 CHECK(xideTasksCount(tasks)==XIDE_TASK_PENDING_LIMIT && xideTasksTest(tasks,&project,NULL,&error)==pending[XIDE_TASK_PENDING_LIMIT-1]);
 CHECK(testConfiguration(&project,program,"wait","over-limit",&error) && !xideTasksTest(tasks,&project,NULL,&error) && strstr(error.message,"queue is full"));
 CHECK(!xideTasksTrimHistory(tasks,0,NULL,0) && xideTasksCount(tasks)==XIDE_TASK_PENDING_LIMIT);
 for(size_t i=1;i<XIDE_TASK_PENDING_LIMIT;++i) xideTaskCancel(pending[i]);
 CHECK(xideTasksTrimHistory(tasks,0,NULL,0)==XIDE_TASK_PENDING_LIMIT-1 && xideTasksCount(tasks)==1);
 xideTaskCancel(pending[0]); CHECK(settled(tasks,pending[0],&info) && info.state==XIDE_TASK_CANCELLED);
 const xide_task* protected[]={pending[0]}; CHECK(!xideTasksTrimHistory(tasks,0,protected,1) && xideTasksContains(tasks,pending[0]));
 logPath=xidePathResolve(NULL,xideTaskTestLog(pending[0])); CHECK(logPath && xidePathExists(logPath));
 for(int i=0;i<20 && !xideTasksTrimHistory(tasks,0,NULL,0);++i) Sleep(2);
 CHECK(!xideTasksCount(tasks) && xidePathExists(logPath));
 CHECK(testConfiguration(&project,argv[0],"","--history-flood",&error)); xide_task* loud=xideTasksTest(tasks,&project,NULL,&error);
 CHECK(loud && settled(tasks,loud,&info) && info.state==XIDE_TASK_SUCCEEDED);
 raw=xideIoReadAllLimit(xideTaskTestLog(loud),8u*1024u*1024u,&rawSize); tail=malloc(1024u*1024u); CHECK(raw && rawSize>3u*1024u*1024u && tail);
 uint64_t cursor=0,dropped=0; size_t used=0,read=xideTaskReadTerminal(loud,&cursor,tail,1024u*1024u,&dropped); used+=read;
 fprintf(stderr,"history ring: raw=%zu retained=%zu dropped=%llu tailEqual=%d\n",rawSize,used,(unsigned long long)dropped,used<=rawSize && !memcmp(tail,raw+rawSize-used,used));
 CHECK(dropped>0 && used==1024u*1024u && !memcmp(tail,raw+rawSize-used,used));
 CHECK(!xideTaskReadTerminal(loud,&cursor,tail,1,&dropped)); xrtFree(raw); raw=NULL; free(tail); tail=NULL;
 app.hidden=1; app.compilerPath=missing; app.viewportWidth=1280; app.viewportHeight=800;
 app.maxFrames=1; CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; CHECK(xideWorkspaceOpen(&app,manifest,1,&error));
 CHECK(xideEditorOpen(&app,entry,&error) && xideEditorOpen(&app,native,&error)); xide_document* cDocument=app.activeDocument;
 CHECK(xuiCodeEditSetText(cDocument->editor,"int native(void) { return 43; }\n")==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK && cDocument->dirty);
 xide_project* live=&app.workspaceModel.projects[0]; xide_task* first=xideTasksBuild(app.tasks,live,NULL,missing,0,&error); CHECK(first && settled(app.tasks,first,&info));
 CHECK(xideTaskViewSelect(&app,first)==XUI_OK); xideDispatchAction(&app,XIDE_ACTION_BUILD); xide_task* automatic=xideTaskViewSelected(&app); CHECK(automatic && automatic!=first && settled(app.tasks,automatic,&info));
 CHECK(xideTaskViewSelect(&app,first)==XUI_OK && xuiCodeEditSetText(cDocument->editor,"int native(void) { return 44; }\n")==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK && cDocument->dirty); uint64_t newest=0;
 for(int i=0;i<400;++i) {
  xide_task* task=xideTasksBuild(app.tasks,live,NULL,missing,0,&error); CHECK(task && settled(app.tasks,task,&info)); newest=info.id;
  xideTaskViewPoll(&app); CHECK(xideTasksCount(app.tasks)<=XIDE_TASK_HISTORY_LIMIT+3 && xideTaskViewSelected(&app)==first && xideTasksContains(app.tasks,automatic));
  if(i==99) plateau=privateBytes();
 }
 after=privateBytes(); CHECK(plateau && after && after<plateau+32u*1024u*1024u);
 CHECK(xideTasksCount(app.tasks)==XIDE_TASK_HISTORY_LIMIT+2 && xuiTreeViewGetNodeCount(app.panels[XIDE_TASKS])==(int)xideTasksCount(app.tasks));
 CHECK(xideTaskViewSelected(&app)==first && cDocument==app.activeDocument && cDocument->dirty && !strcmp(cDocument->language,"c") && strstr(xuiCodeEditGetText(cDocument->editor),"44"));
 xide_task* last=xideTasksAt(app.tasks,xideTasksCount(app.tasks)-1); xideTaskInfo(last,&info); CHECK(info.id==newest && xideTaskViewSelect(&app,last)==XUI_OK); xideTaskViewPoll(&app);
 CHECK(strstr(xuiTextEditGetText(app.panels[XIDE_OUTPUT]),"Task") && xideTaskViewSelect(&app,loud)==XUI_ERROR_INVALID_ARGUMENT);
 CHECK(xuiTreeViewGetSelected(app.panels[XIDE_TASKS])==(int)xideTasksCount(app.tasks));
 xide_task* middle=xideTasksAt(app.tasks,10); CHECK(middle && xideTaskViewSelect(&app,middle)==XUI_OK);
 xide_task* next=xideTasksBuild(app.tasks,live,NULL,missing,0,&error); CHECK(next && settled(app.tasks,next,&info)); newest=info.id; xideTaskViewPoll(&app);
 CHECK(xideTaskViewSelected(&app)==middle);
 int selected=xuiTreeViewGetSelected(app.panels[XIDE_TASKS]); CHECK(selected>0 && xideTasksAt(app.tasks,(size_t)selected-1)==middle);
 const xui_tree_view_node_t* node=xuiTreeViewGetNode(app.panels[XIDE_TASKS],xuiTreeViewGetNodeCount(app.panels[XIDE_TASKS])-1); char id[64]; snprintf(id,sizeof(id),"#%llu",(unsigned long long)newest); CHECK(node && strstr(node->sText,id));
 cImage=xidePathResolve(root,"history-c.png"); xlImage=xidePathResolve(root,"history-xlang.png"); CHECK(cImage && xlImage && capture(&app,cImage));
 CHECK(xideEditorOpen(&app,entry,&error) && !strcmp(app.activeDocument->language,"xlang") && capture(&app,xlImage));
 listImage=xidePathResolve(root,"history-list.png"); xui_dock_window_info_t dock={0}; dock.iSize=sizeof(dock);
 CHECK(listImage && xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_TASKS],&dock)==XUI_OK && xuiDockPanelSetPaneActiveWindow(app.dock,dock.iPane,app.windows[XIDE_TASKS])==XUI_OK && capture(&app,listImage));
 printf("Task history checks: %d passed; 400 completed requests, retained=%zu, privateBytes=%llu->%llu, raw flood=%zu bytes\n",checks,xideTasksCount(app.tasks),(unsigned long long)plateau,(unsigned long long)after,rawSize);
cleanup:
 xgeQuit(); if(began) xgeEnd(); xideAppDestroy(&app); xideTasksDestroy(tasks); xideProjectClear(&project); xrtFree(raw); free(tail);
 char* paths[]={root,manifest,entry,native,binary,program,missing,logPath,cImage,xlImage,listImage}; for(size_t i=0;i<sizeof(paths)/sizeof(*paths);++i) free(paths[i]); return failed;
}
typedef struct history_fixture { int argc; char** argv; HDESK desktop; int started,failed; } history_fixture;
static int verify(void* user) { history_fixture* f=user; f->started=1; f->failed=runChecks(f->argc,f->argv); return XGE_OK; }
static DWORD WINAPI worker(void* user)
{
 history_fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1;
 xge_desc_t d={0}; d.iWidth=1280; d.iHeight=800; d.sTitle="xlang task history verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP;
 int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1;
}
int main(int argc,char** argv)
{
 if(argc==2 && !strcmp(argv[1],"--history-flood")) return flood();
 if(argc!=2) return 2;
 history_fixture f={.argc=argc,.argv=argv}; wchar_t name[100]; swprintf(name,100,L"XlangTaskHistory-%lu",GetCurrentProcessId());
 f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1;
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1;
 if(thread) { if(WaitForSingleObject(thread,120000)!=WAIT_OBJECT_0) ExitProcess(1); if(!GetExitCodeThread(thread,&code)) code=1; CloseHandle(thread); }
 CloseDesktop(f.desktop); return (int)code;
}
