#include "services/tasks.h"
#include "services/file_rebind.h"
#include "services/project_migration.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"project_test_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,info.error); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return v ? xrtValueObjectGet(v,sv(k)) : NULL; }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s))); }
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int configure(xide_project* p,const char* program,const char* mode,xide_model_error* e)
{
    xvalue* configs=xideProjectConfigurationsJson(p);
    xvalue* test=xrtJsonParse(sv("{\"workingDirectory\":\"work 中文\",\"arguments\":[\"中文 参数\",\"a\\\"b\",\"尾\\\\\",\"\"],\"environment\":{},\"futureTest\":{\"keep\":17}}"));
    int okay=configs && test && put(test,"program",program) && put(get(test,"environment"),"XIDE_TASK_MODE",mode);
    if(okay) okay=xrtValueObjectSetNew(xrtValueArrayGet(configs,0),sv("test"),test); else xrtValueRelease(test);
    if(okay) okay=xideProjectApplyConfigurations(p,configs,p->activeConfiguration,e) && xideProjectSave(p,NULL,e);
    xrtValueRelease(configs); return okay;
}
static int waitTask(xide_tasks* ts,xide_task* t,xide_task_info* i,int input)
{
    uint64_t end=GetTickCount64()+10000; int sent=0;
    do {
        xideTasksPoll(ts); xideTaskInfo(t,i);
        if(input && !sent && xideTaskInputOpen(t)) { if(!xideTaskWrite(t,"测试输入😀\n",strlen("测试输入😀\n"))) return 0; xideTaskCloseInput(t); sent=1; }
        if(i->state>=XIDE_TASK_SUCCEEDED) return 1;
        Sleep(5);
    } while(GetTickCount64()<end); return 0;
}
static int running(xide_task* t)
{
    uint64_t end=GetTickCount64()+10000; xide_task_info info={0};
    do { xideTaskInfo(t,&info); if(info.tested && xideTaskInputOpen(t)) return 1; if(info.state>=XIDE_TASK_SUCCEEDED) return 0; Sleep(5); } while(GetTickCount64()<end); return 0;
}
int main(int argc,char** argv)
{
    int failed=0,checks=0; xide_model_error error={0}; xide_task_info info={0}; xide_project project={0},loaded={0}; xide_workspace workspace={0};
    xide_tasks* tasks=NULL; xvalue *configs=NULL,*before=NULL; xide_file_rebind* rebind=NULL; xide_project_migration* migration=NULL;
    char *root=NULL,*manifest=NULL,*entry=NULL,*cwd=NULL,*program=NULL,*binaryDir=NULL,*fixture=NULL,*copyPath=NULL,*renamed=NULL,*moved=NULL; unsigned char* raw=NULL;
    CHECK(argc==2 || argc==3); root=xidePathResolve(argv[1],"中文 项目测试"); CHECK(root && xrtDirCreateAll(root));
    manifest=xidePathResolve(root,"项目.xlp"); entry=xidePathResolve(root,"main.xl"); cwd=xidePathResolve(root,"work 中文"); program=xidePathResolve(root,"测试 程序.exe");
    binaryDir=xidePathDirectory(argv[0]); fixture=binaryDir ? xidePathResolve(binaryDir,"xide_task_program.exe") : NULL;
    copyPath=xidePathResolve(root,"copy/project.xlp"); renamed=xidePathResolve(root,"重命名.exe"); moved=xidePathResolve(argv[1],"迁移后 项目测试");
    CHECK(manifest && entry && cwd && program && fixture && copyPath && renamed && moved && xrtDirCreateAll(cwd) && writeText(entry,"func main(...) as int\n return 0\nend\n") && xrtFileCopy(fixture,program,true));
    CHECK(xideProjectCreate(&project,manifest,"测试项目",entry,&error) && xideProjectSetMetadata(&project,"测试项目","测试宿主", "dll",&error) && xideProjectSave(&project,NULL,&error));
    tasks=xideTasksCreate(); CHECK(tasks && !xideTasksCount(tasks) && !xideTasksTest(tasks,&project,NULL,&error) && !xideTasksCount(tasks));
    CHECK(configure(&project,"测试 程序.exe","echo",&error) && !xideTasksCount(tasks));
    CHECK(project.configurations[0].test.arguments.count==4 && xidePathEqual(project.configurations[0].test.program,program) && xidePathEqual(project.configurations[0].test.workingDirectory,cwd));
    xide_task* task=xideTasksTest(tasks,&project,project.activeConfiguration,&error); CHECK(task && xideTasksTest(tasks,&project,project.activeConfiguration,&error)==task && xideTasksCount(tasks)==1);
    xideProjectClear(&project); CHECK(waitTask(tasks,task,&info,1) && info.state==XIDE_TASK_SUCCEEDED && info.kind==XIDE_TASK_KIND_TEST && info.tested && !info.built && !info.ran && !*xideTaskOutput(task) && !*xideTaskArtifact(task) && !*xideTaskBuildLog(task) && !*xideTaskRunLog(task));
    size_t size=0; raw=xrtFileReadAll(xideTaskTestLog(task),&size); CHECK(raw && size && strstr((char*)raw,"ARG[中文 参数]") && strstr((char*)raw,"ARG[a\"b]") && strstr((char*)raw,"ARG[尾\\]") && strstr((char*)raw,"ARG[]") && strstr((char*)raw,"测试输入😀") && strstr((char*)raw,"work 中文")); xrtFree(raw); raw=NULL;
    CHECK(xideProjectLoad(&project,manifest,&error) && configure(&project,"测试 程序.exe","fail",&error));
    task=xideTasksTest(tasks,&project,NULL,&error); CHECK(task && waitTask(tasks,task,&info,0) && info.state==XIDE_TASK_FAILED && info.tested && info.exit.Code==13 && strstr(info.error,"Test program"));
    CHECK(configure(&project,"missing.exe","echo",&error)); task=xideTasksTest(tasks,&project,NULL,&error); CHECK(task && waitTask(tasks,task,&info,0) && info.state==XIDE_TASK_FAILED && !info.tested);
    CHECK(configure(&project,"uninvoked.bat","echo",&error)); size_t count=xideTasksCount(tasks); CHECK(!xideTasksTest(tasks,&project,NULL,&error) && xideTasksCount(tasks)==count);
    CHECK(configure(&project,"测试 程序.exe","wait",&error)); task=xideTasksTest(tasks,&project,NULL,&error); CHECK(task && running(task));
    configs=xideProjectConfigurationsJson(&project); CHECK(configs);
    xvalue* second=xrtValueDeepClone(xrtValueArrayGet(configs,0)); CHECK(second && put(second,"id","second") && put(get(get(second,"test"),"environment"),"XIDE_TASK_MODE","echo") && xrtValueArrayAppendNew(configs,second) && xideProjectApplyConfigurations(&project,configs,project.activeConfiguration,&error) && xideProjectSave(&project,NULL,&error));
    xrtValueRelease(configs); configs=NULL; xide_task* queued=xideTasksTest(tasks,&project,"second",&error); CHECK(queued && queued!=task); xideTaskInfo(queued,&info); CHECK(info.state==XIDE_TASK_QUEUED && !info.tested);
    xideTasksInvalidateInput(tasks,entry); xideTaskInfo(task,&info); CHECK(!info.invalidated); xideTaskInfo(queued,&info); CHECK(info.invalidated);
    xideTaskCancel(task); CHECK(waitTask(tasks,task,&info,0) && info.state==XIDE_TASK_CANCELLED && waitTask(tasks,queued,&info,0) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.tested);
    task=xideTasksTest(tasks,&project,NULL,&error); CHECK(task && running(task)); queued=xideTasksTest(tasks,&project,"second",&error); CHECK(queued); xideTaskCancel(queued); xideTaskInfo(queued,&info); CHECK(info.state==XIDE_TASK_CANCELLED && !info.tested); xideTaskCancel(task); CHECK(waitTask(tasks,task,&info,0) && !xideTasksBusy(tasks));
    project.dirty=1; CHECK(!xideTasksTest(tasks,&project,NULL,&error)); project.dirty=0;
    configs=xideProjectConfigurationsJson(&project); before=xrtValueDeepClone(configs); CHECK(configs && before);
    CHECK(put(get(get(xrtValueArrayGet(configs,0),"test"),"environment"),"xide_task_mode","duplicate") && !xideProjectApplyConfigurations(&project,configs,project.activeConfiguration,&error) && error.code==XIDE_MODEL_INVALID); xrtValueRelease(configs); configs=xideProjectConfigurationsJson(&project); CHECK(xrtValueEqual(before,configs));
    CHECK(xrtValueObjectSetNew(get(xrtValueArrayGet(configs,0),"test"),sv("program"),xrtValueInt(7)) && !xideProjectApplyConfigurations(&project,configs,project.activeConfiguration,&error)); xrtValueRelease(configs); configs=NULL;
    char* parent=xidePathDirectory(copyPath); CHECK(parent && xrtDirCreateAll(parent)); free(parent);
    CHECK(xideProjectLoad(&loaded,manifest,&error) && xideProjectSave(&loaded,copyPath,&error) && xideProjectLoad(&loaded,copyPath,&error) && xidePathEqual(loaded.configurations[0].test.program,program) && xidePathEqual(loaded.configurations[0].test.workingDirectory,cwd));
    configs=xideProjectConfigurationsJson(&loaded); CHECK(get(get(xrtValueArrayGet(configs,0),"test"),"futureTest")); xrtValueRelease(configs); configs=NULL;
    CHECK(xideWorkspaceCreate(&workspace,"测试路径事务",&error) && xideWorkspaceAddProject(&workspace,manifest,"alpha",&error) && xrtFileCopy(program,renamed,true));
    CHECK(xideFileRebindPrepare(&workspace,program,renamed,&rebind,&error) && rebind && xideFileRebindProjectCount(rebind)==1); xideFileRebindApply(rebind);
    xide_project* bound=xideWorkspaceFindProject(&workspace,"alpha"); CHECK(bound && xidePathEqual(bound->configurations[0].test.program,renamed) && xideProjectSave(bound,NULL,&error));
    migration=xideProjectMigrationPrepare(&workspace,"alpha",moved,XIDE_MIGRATE_DIRECTORY,&error); CHECK(migration && xideProjectMigrationCommit(migration,&error));
    char* expected=xidePathResolve(moved,"重命名.exe"); CHECK(expected && xidePathEqual(bound->configurations[0].test.program,expected)); free(expected); expected=xidePathResolve(moved,"work 中文"); CHECK(expected && xidePathEqual(bound->configurations[0].test.workingDirectory,expected)); free(expected);
    CHECK(xideProjectSave(bound,NULL,&error) && xideProjectLoad(&loaded,bound->manifest,&error) && xidePathEqual(loaded.configurations[0].test.program,bound->configurations[0].test.program));
    if(argc==3) {
        xideProjectClear(&project); char* realManifest=xidePathResolve(moved,"real.xlp"),*realEntry=xidePathResolve(moved,"main.xl"),*realC=xidePathResolve(moved,"native.c");
        int okay=realManifest && realEntry && realC && writeText(realEntry,"extern \"native_answer\" func nativeAnswer() as c_int\nfunc main(...) as int\n return nativeAnswer()-42\nend\n") && writeText(realC,"int native_answer(void) { return 42; }\n") && xideProjectCreate(&project,realManifest,"Real C/xlang test",realEntry,&error) && xideProjectAddFile(&project,realC,"c",&error) && xideProjectSave(&project,NULL,&error);
        free(realManifest); free(realEntry); free(realC); CHECK(okay);
        task=xideTasksBuild(tasks,&project,project.activeConfiguration,argv[2],0,&error); CHECK(task && waitTask(tasks,task,&info,0) && info.state==XIDE_TASK_SUCCEEDED && info.built && !info.ran);
        char* output=xidePathRelative(moved,xideTaskOutput(task)); CHECK(output && configure(&project,output,"echo",&error)); free(output);
        task=xideTasksTest(tasks,&project,project.activeConfiguration,&error); CHECK(task && waitTask(tasks,task,&info,0) && info.state==XIDE_TASK_SUCCEEDED && info.tested && !info.built && !info.ran && info.exit.Code==0);
        puts("project_test_test compiler passed: actual saved C/xlang project built, then explicit configured PE test process exited 0 without an implicit rebuild.");
    }
    printf("project_test_test passed: %d checks; explicit DLL test host without compiler/build, Unicode argv/cwd/empty argument/stdin, exit 13/start failure, duplicate/serial/cancel/stale queued input, no automatic execution, environment validation, unknown fields, save-as and actual directory migration.\n",checks);
cleanup:
    xideTasksDestroy(tasks); xideProjectClear(&project); xideProjectClear(&loaded); xideWorkspaceClear(&workspace); xrtValueRelease(configs); xrtValueRelease(before); xideFileRebindDestroy(rebind); xideProjectMigrationDestroy(migration); xrtFree(raw);
    free(root); free(manifest); free(entry); free(cwd); free(program); free(binaryDir); free(fixture); free(copyPath); free(renamed); free(moved); return failed;
}
