#define _WIN32_WINNT 0x0601
#include "services/project_templates.h"
#include "services/tasks.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"template_test:%d: %s; %s\n",__LINE__,#v,error.message); failed=1; goto cleanup; } } while(0)
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static wchar_t* wide(const char* s) { int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0); wchar_t* w=n ? malloc((size_t)n*sizeof(*w)) : NULL; if(w) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n); return w; }
static int waitTask(xide_tasks* tasks,xide_task* task,xide_task_info* info)
{ uint64_t until=GetTickCount64()+45000; do { xideTasksPoll(tasks); xideTaskInfo(task,info); if(info->state>=XIDE_TASK_SUCCEEDED) return 1; Sleep(5); } while(GetTickCount64()<until); return 0; }
int main(int argc,char** argv)
{
 int failed=0; xide_model_error error={0}; xide_workspace workspace={0},loaded={0}; xide_template_plan* plan=NULL; xide_project project={0}; xide_tasks* tasks=NULL; HANDLE lock=INVALID_HANDLE_VALUE; HMODULE dll=NULL;
 char *root=NULL,*manifest=NULL,*chosen=NULL,*path=NULL,*id=NULL,*firstId=NULL,*collision=NULL; xvalue* before=NULL; wchar_t* w=NULL; unsigned char* bytes=NULL;
 CHECK(argc==2 || argc==3); id=xideCreateId("模板 验收😀"); root=id ? xidePathResolve(argv[1],id) : NULL; free(id); id=NULL; CHECK(root && xrtDirCreateAll(root));
 manifest=xidePathResolve(root,"工作空间.xlw"); CHECK(manifest && xideWorkspaceCreate(&workspace,"模板工作空间",&error) && xideWorkspaceSave(&workspace,manifest,&error));
 CHECK(xideProjectTemplateCount()==4 && !xideProjectTemplateFind("unknown") && !xideProjectTemplateAt(4));
 const char* invalid[]={"",".","..","../escape","a/b","a\\b","a:b","a*","a?","a\"b","a<b","a|b","a\n","CON","nul.txt","com9.xl","LPT1","COM¹","CONIN$","项目."," 项目","项目 ","\xff"};
 for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i) { plan=xideTemplatePlanPrepare(root,invalid[i],"console",&error); CHECK(!plan && error.code==XIDE_MODEL_INVALID); }
 plan=xideTemplatePlanPrepare(root,"有效项目","unknown",&error); CHECK(!plan);
 const char* names[]={"空白应用😀","控制台 应用","混合工程 中文","动态库项目"};
 for(size_t i=0;i<xideProjectTemplateCount();++i) {
  const xide_project_template* d=xideProjectTemplateAt(i); plan=xideTemplatePlanPrepare(root,names[i],d->id,&error); CHECK(plan && !xidePathExists(xideTemplatePlanDirectory(plan)) && strstr(xideTemplatePlanManifest(plan),"xlang-project-1"));
  const xide_project* prepared=xideTemplatePlanProject(plan); CHECK(prepared && prepared->files.count==d->fileCount && !strcmp(prepared->name,names[i]));
  CHECK(xideTemplatePlanCommit(&workspace,plan,i==0,&chosen,&error) && chosen && workspace.projectCount==i+1 && workspace.dirty);
  CHECK(!xideTemplatePlanCommit(&workspace,plan,0,NULL,&error)); xideTemplatePlanDestroy(plan); plan=NULL;
  xide_project* p=xideWorkspaceFindProject(&workspace,chosen); CHECK(p && !p->dirty && !strcmp(p->kind,d->kind) && xideProjectLoad(&project,p->manifest,&error) && project.files.count==d->fileCount);
  if(i==0) { firstId=malloc(strlen(chosen)+1); CHECK(firstId); strcpy(firstId,chosen); } else CHECK(!strcmp(workspace.startupProject,firstId));
  for(size_t j=0;j<d->fileCount;++j) { path=xidePathDirectory(p->manifest); CHECK(path); char* file=xidePathResolve(path,d->files[j].path); free(path); path=file; CHECK(path); size_t length=0; bytes=xrtFileReadAll(path,&length); CHECK(bytes && length==strlen(d->files[j].content) && !memcmp(bytes,d->files[j].content,length)); xrtFree(bytes); bytes=NULL; free(path); path=NULL; }
  CHECK(p->cSources.count==(i==2 ? 1u : 0u) && !p->resources.count && !strcmp(xidePathFilename(p->configurations[0].output),i==3 ? "app.dll" : "app.exe"));
  if(argc==3) {
   if(!tasks) { tasks=xideTasksCreate(); } CHECK(tasks); xide_task* task=xideTasksBuild(tasks,p,p->activeConfiguration,argv[2],i!=3,&error); xide_task_info info={0}; CHECK(task && waitTask(tasks,task,&info));
   if(info.state!=XIDE_TASK_SUCCEEDED) fprintf(stderr,"template %s compiler output:\n%s\n",d->id,xideTaskOutput(task));
   CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran==(i!=3));
   if(i==1 || i==2) { uint64_t cursor=0,dropped=0; char output[4096]; size_t n=xideTaskReadTerminal(task,&cursor,output,sizeof(output)-1,&dropped); output[n]=0; CHECK(!dropped && strstr(output,i==1 ? "Hello, xlang!" : "C_XLANG_OK")); }
   if(i==3) { w=wide(xideTaskArtifact(task)); CHECK(w); dll=LoadLibraryW(w); free(w); w=NULL; if(!dll) fprintf(stderr,"DLL load Windows error: %lu\n",GetLastError()); CHECK(dll); FARPROC symbol=GetProcAddress(dll,"xlang_add"); int (*add)(int,int)=NULL; CHECK(symbol && sizeof(add)==sizeof(symbol)); memcpy(&add,&symbol,sizeof(add)); CHECK(add(20,22)==42 && add(-10,3)==-7); FreeLibrary(dll); dll=NULL; CHECK(!xideTasksBuild(tasks,p,p->activeConfiguration,argv[2],1,&error)); }
  }
  free(chosen); chosen=NULL; xideProjectClear(&project);
 }
 CHECK(xideWorkspaceSave(&workspace,NULL,&error) && xideWorkspaceLoad(&loaded,manifest,&error) && loaded.projectCount==4 && !strcmp(loaded.startupProject,firstId));
 before=xideWorkspaceExport(&workspace,&error); CHECK(before);
 collision=xidePathResolve(root,"已存在目录"); CHECK(collision && xrtDirCreateAll(collision)); path=xidePathResolve(collision,"main.xl"); CHECK(path && writeText(path,"EXISTING\n"));
 plan=xideTemplatePlanPrepare(root,"已存在目录","console",&error); CHECK(!plan && error.code==XIDE_MODEL_CONFLICT); size_t length=0; bytes=xrtFileReadAll(path,&length); CHECK(bytes && length==9 && !memcmp(bytes,"EXISTING\n",9)); xrtFree(bytes); bytes=NULL; free(path); path=NULL; free(collision); collision=NULL;
 path=xidePathResolve(root,"文件冲突"); CHECK(path && writeText(path,"ORIGINAL\n")); plan=xideTemplatePlanPrepare(root,"文件冲突","blank",&error); CHECK(!plan && error.code==XIDE_MODEL_CONFLICT); free(path); path=NULL;
 plan=xideTemplatePlanPrepare(root,"预览后出现","mixed-c",&error); CHECK(plan); collision=xidePathResolve(root,"预览后出现"); CHECK(collision && xrtDirCreateAll(collision) && !xideTemplatePlanCommit(&workspace,plan,1,&chosen,&error) && !chosen && workspace.projectCount==4 && !workspace.dirty); xideTemplatePlanDestroy(plan); plan=NULL; free(collision); collision=NULL;
 plan=xideTemplatePlanPrepare(root,"访问被锁定","console",&error); CHECK(plan); w=wide(root); CHECK(w); lock=CreateFileW(w,DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL); free(w); w=NULL; CHECK(lock!=INVALID_HANDLE_VALUE && !xideTemplatePlanCommit(&workspace,plan,1,&chosen,&error) && !chosen && !xidePathExists(xideTemplatePlanDirectory(plan))); CloseHandle(lock); lock=INVALID_HANDLE_VALUE; xideTemplatePlanDestroy(plan); plan=NULL;
 xvalue* after=xideWorkspaceExport(&workspace,&error); int unchanged=after && xrtValueEqual(before,after); xrtValueRelease(after); CHECK(unchanged && !workspace.dirty && !strcmp(workspace.startupProject,firstId));
 if(argc==3) puts("template_test real DEMO6 passed: generated blank/console apps, actual C extern/header mixed call and run, DLL build/load/export invocation and no standalone DLL run.");
 puts("template_test passed: four complete plans, Unicode names, preview without writes, exact generated bytes and manifest roundtrip, C/header/config/identity, startup preservation, device/path/encoding guards, existing directory/file and preview races, parent lock and unchanged workspace.");
cleanup:
 if(lock!=INVALID_HANDLE_VALUE) { CloseHandle(lock); }
 if(dll) { FreeLibrary(dll); } xideTemplatePlanDestroy(plan); xideTasksDestroy(tasks); xideProjectClear(&project); xideWorkspaceClear(&workspace); xideWorkspaceClear(&loaded); xrtValueRelease(before); xrtFree(bytes); free(w); free(root); free(manifest); free(chosen); free(path); free(id); free(firstId); free(collision); return failed;
}