#include "services/tasks.h"
#include "platform/path.h"
#include "platform/task_stage.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(test) do { if(!(test)) { fprintf(stderr,"task_test:%d: %s; model=%s; task=%s\n",__LINE__,#test,error.message,info.error); failed=1; goto cleanup; } } while(0)
static char* utf8(const wchar_t* text)
{
    int count=WideCharToMultiByte(CP_UTF8,0,text,-1,NULL,0,NULL,NULL); char* value=malloc((size_t)count);
    if(value) WideCharToMultiByte(CP_UTF8,0,text,-1,value,count,NULL,NULL);
    return value;
}
static void emit(const char* text)
{
    DWORD written; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,(DWORD)strlen(text),&written,NULL);
}
static int helper(int count,char** arguments)
{
    if(count==2 && !strcmp(arguments[1],"--help")) { emit("xl build <file.xlp> -o <output> --inc-path --lib-path --link\n"); return 0; }
    if(count>=5 && !strcmp(arguments[1],"build")) {
        xvalue* manifest=xrtJsonParseFile(arguments[2]); xstrview source={0};
        if(!manifest || !xrtValueGetString(xrtValueObjectGet(manifest,(xstrview){"entry",5}),&source)) return 2;
        char* entry=malloc(source.Size+1); if(!entry) return 3; memcpy(entry,source.Data,source.Size); entry[source.Size]=0;
        size_t size=0; unsigned char* raw=xrtFileReadAll(entry,&size); if(!raw) return 4;
        char* text=malloc(size+1); if(!text) { xrtFree(raw); return 6; } memcpy(text,raw,size); text[size]=0; xrtFree(raw);
        int result=0;
        if(strstr(text,"FAKE_GATE")) {
            wchar_t name[128]; DWORD length=GetEnvironmentVariableW(L"XIDE_TEST_BUILD_GATE",name,128);
            HANDLE gate=length && length<128 ? OpenEventW(SYNCHRONIZE,FALSE,name) : NULL;
            if(!gate) { result=10; goto finished; }
            emit("BUILD_GATE_WAITING\n"); DWORD waited=WaitForSingleObject(gate,30000); CloseHandle(gate);
            if(waited!=WAIT_OBJECT_0) { result=10; goto finished; }
        }
        if(strstr((char*)text,"FAKE_SLOW")) { emit("BUILD_STARTED\n"); Sleep(800); }
        if(strstr((char*)text,"FAKE_FAIL")) { emit(entry); emit(":3:5: [semantic] 真实失败 fixture\n"); result=9; }
        else if(!strstr((char*)text,"FAKE_NONE")) { char* binaryDirectory=xidePathDirectory(arguments[0]); char* binary=binaryDirectory ? xidePathResolve(binaryDirectory,"xide_task_program.exe") : NULL;
            if(!binary || !xrtFileCopy(binary,arguments[4],true)) result=5;
            free(binaryDirectory); free(binary); }
finished:
        free(entry); free(text); xrtValueRelease(manifest); return result;
    }
    if(strstr(arguments[0],"program.exe")) {
        wchar_t mode[128]={0}; GetEnvironmentVariableW(L"XIDE_TASK_MODE",mode,128);
        for(int i=1;i<count;++i) { emit("ARG["); emit(arguments[i]); emit("]\n"); }
        wchar_t current[32768]; GetCurrentDirectoryW(32768,current); char* directory=utf8(current); emit("DIR["); emit(directory); emit("]\n"); free(directory);
        emit("RUN_STARTED\n");
        if(!wcscmp(mode,L"wait")) { Sleep(60000); return 0; }
        if(!wcscmp(mode,L"fail")) return 13;
        char input[1024]; DWORD size,written;
        while(ReadFile(GetStdHandle(STD_INPUT_HANDLE),input,sizeof(input),&size,NULL) && size) WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),input,size,&written,NULL);
        return 0;
    }
    return -1;
}
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,strlen(text)}); }
static int completedStage(xide_task* task)
{
    char* directory=xidePathDirectory(xideTaskBuildLog(task)); char* marker=directory ? xidePathResolveLexical(directory,".xide-stage") : NULL; wchar_t* wide=marker ? xidePathNativeWide(marker) : NULL;
    HANDLE h=wide ? CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL) : INVALID_HANDLE_VALUE; unsigned char bytes[848]; DWORD count=0;
    int okay=h!=INVALID_HANDLE_VALUE && ReadFile(h,bytes,sizeof(bytes),&count,NULL) && count==sizeof(bytes) && !memcmp(bytes,"xlang-ide-stage1",16) && bytes[20]==2;
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    free(directory); free(marker); free(wide); return okay;
}
static int loadProject(xide_project* project,const char* manifest,const char* mode,xide_model_error* error)
{
    char json[2048]; snprintf(json,sizeof(json),"{\"format\":\"xlang-project-1\",\"kind\":\"exe\",\"entry\":\"main.xl\",\"ide\":{\"format\":\"xlang-ide-project-1\",\"name\":\"任务 工程\",\"configurations\":[{\"id\":\"dev\",\"name\":\"开发\",\"output\":\"build/app.exe\",\"run\":{\"workingDirectory\":\".\",\"arguments\":[\"中文 参数\",\"a\\\"b\",\"尾\\\\\"],\"environment\":{\"XIDE_TASK_MODE\":\"%s\"}}}]}}",mode);
    return writeText(manifest,json) && xideProjectLoad(project,manifest,error);
}
static int waitTask(xide_tasks* tasks,xide_task* task,xide_task_info* info,unsigned timeout,int closeInput)
{
    ULONGLONG began=GetTickCount64(); int sent=0;
    for(;;) {
        xideTasksPoll(tasks); xideTaskInfo(task,info);
        if(info->phase==XIDE_TASK_RUN && closeInput && !sent && xideTaskWrite(task,"终端输入\n",strlen("终端输入\n"))) { xideTaskCloseInput(task); sent=1; }
        if(info->state>=XIDE_TASK_SUCCEEDED) return 1;
        if(GetTickCount64()-began>timeout) return 0;
        Sleep(5);
    }
}
static int waitPhase(xide_task* task,enum xide_task_phase phase,xide_task_info* info,unsigned timeout)
{
    ULONGLONG began=GetTickCount64();
    do { xideTaskInfo(task,info); if(info->phase==phase) return 1; if(info->state>=XIDE_TASK_SUCCEEDED) return 0; Sleep(5); } while(GetTickCount64()-began<timeout); return 0;
}
int main(void)
{
    int count=0; wchar_t** wideArguments=CommandLineToArgvW(GetCommandLineW(),&count); char** arguments=calloc((size_t)count+1,sizeof(*arguments));
    if(!arguments) return 1;
    for(int i=0;i<count;++i) arguments[i]=utf8(wideArguments[i]);
    LocalFree(wideArguments);
    int result=helper(count,arguments);
    if(result!=-1) { for(int i=0;i<count;++i) free(arguments[i]); free(arguments); return result; }
    int failed=0; xide_model_error error={0}; xide_task_info info={0}; xide_project project={0}; xide_tasks* tasks=NULL;
    char *root=NULL,*manifest=NULL,*entry=NULL,*output=NULL,*include=NULL,*library=NULL,*native=NULL,*alias=NULL; xide_diagnostic diagnostic={0}; HANDLE locked=INVALID_HANDLE_VALUE; unsigned char *bytes=NULL,*old=NULL; size_t size=0;
    CHECK(count==2 || count==3); tasks=xideTasksCreate(); CHECK(tasks);
    root=xidePathResolve(arguments[1],"中文 构建运行"); CHECK(root && xrtDirCreateAll(root)); manifest=xidePathResolve(root,"项目.xlp"); entry=xidePathResolve(root,"main.xl"); CHECK(manifest && entry);
    CHECK(writeText(entry,"FAKE_OK\n") && loadProject(&project,manifest,"echo",&error)); output=xidePathResolve(root,"build/app.exe"); CHECK(output);
    xide_task* task=xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error); CHECK(task);
    CHECK(xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error)==task && xideTasksCount(tasks)==1);
    xideProjectClear(&project); /* Workers must not borrow the live model. */
    CHECK(waitTask(tasks,task,&info,10000,1)); CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && xidePathExists(output));
    uint64_t cursor=0,dropped=0; char log[16384]={0}; size=xideTaskReadTerminal(task,&cursor,log,sizeof(log)-1,&dropped); log[size]=0;
    CHECK(!dropped && strstr(log,"ARG[中文 参数]") && strstr(log,"ARG[a\"b]") && strstr(log,"终端输入") && strstr(log,"中文 构建运行"));
    CHECK(xidePathExists(xideTaskBuildLog(task)) && xidePathExists(xideTaskRunLog(task)) && !xidePathEqual(xideTaskArtifact(task),output));
    CHECK(completedStage(task));
    bytes=xrtFileReadAll(output,&size); CHECK(bytes && size>1000); size_t oldSize=size; old=bytes; bytes=NULL;
    CHECK(writeText(entry,"FAKE_FAIL\n") && loadProject(&project,manifest,"echo",&error));
    task=xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error); CHECK(task && waitTask(tasks,task,&info,10000,0));
    CHECK(info.state==XIDE_TASK_FAILED && !info.built && !info.ran && info.exit.Code==9 && xideTaskDiagnosticCount(task)==1);
    CHECK(completedStage(task));
    CHECK(xideTaskDiagnostic(task,0,&diagnostic) && xidePathEqual(diagnostic.path,entry) && diagnostic.line==3 && diagnostic.column==5 && strstr(diagnostic.message,"真实失败")); xideDiagnosticClear(&diagnostic);
    bytes=xrtFileReadAll(output,&size); CHECK(bytes && size==oldSize && !memcmp(bytes,old,size)); xrtFree(bytes); bytes=NULL; xrtFree(old); old=NULL;
    CHECK(writeText(entry,"FAKE_NONE\n")); task=xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error); CHECK(task && waitTask(tasks,task,&info,10000,0)); CHECK(info.state==XIDE_TASK_FAILED && !info.ran && strstr(info.error,"without producing"));
    CHECK(writeText(entry,"FAKE_SLOW\n")); task=xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error); CHECK(task && waitPhase(task,XIDE_TASK_BUILD,&info,5000));
    xide_task* queued=xideTasksBuild(tasks,&project,"dev",arguments[0],0,&error); CHECK(queued && queued!=task); xideTaskInfo(queued,&info); CHECK(info.state==XIDE_TASK_QUEUED);
    xideTaskCancel(queued); xideTaskCancel(task); CHECK(waitTask(tasks,task,&info,5000,0) && info.state==XIDE_TASK_CANCELLED && !info.ran); xideTaskInfo(queued,&info); CHECK(info.state==XIDE_TASK_CANCELLED && !xideTasksBusy(tasks));
    task=xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error); CHECK(task && waitPhase(task,XIDE_TASK_BUILD,&info,5000));
    xideTasksInvalidateInput(tasks,entry); CHECK(waitTask(tasks,task,&info,5000,0) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.ran);
    task=xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error); CHECK(task && waitPhase(task,XIDE_TASK_BUILD,&info,5000));
    char aliasName[128]; snprintf(aliasName,sizeof(aliasName),"alias-%llu.xl",(unsigned long long)GetTickCount64()); alias=xidePathResolve(root,aliasName); CHECK(alias);
    int entryCount=MultiByteToWideChar(CP_UTF8,0,entry,-1,NULL,0),aliasCount=MultiByteToWideChar(CP_UTF8,0,alias,-1,NULL,0);
    wchar_t* entryWide=malloc((size_t)entryCount*sizeof(*entryWide)); wchar_t* aliasWide=malloc((size_t)aliasCount*sizeof(*aliasWide)); CHECK(entryWide && aliasWide);
    MultiByteToWideChar(CP_UTF8,0,entry,-1,entryWide,entryCount); MultiByteToWideChar(CP_UTF8,0,alias,-1,aliasWide,aliasCount);
    int linked=CreateHardLinkW(aliasWide,entryWide,NULL); free(entryWide); free(aliasWide); CHECK(linked);
    xideTasksInvalidateInput(tasks,alias); CHECK(waitTask(tasks,task,&info,5000,0) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.ran);
    task=xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error); CHECK(task && waitPhase(task,XIDE_TASK_BUILD,&info,5000)); Sleep(100); CHECK(writeText(entry,"FAKE_OK\n"));
    CHECK(waitTask(tasks,task,&info,5000,0) && info.state==XIDE_TASK_FAILED && !info.ran && strstr(info.error,"changed while"));
    CHECK(loadProject(&project,manifest,"fail",&error)); task=xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error); CHECK(task && waitTask(tasks,task,&info,10000,0)); CHECK(info.state==XIDE_TASK_FAILED && info.built && info.ran && info.exit.Code==13);
    CHECK(loadProject(&project,manifest,"wait",&error)); task=xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error); CHECK(task && waitPhase(task,XIDE_TASK_RUN,&info,10000)); Sleep(100); xideTaskCancel(task); CHECK(waitTask(tasks,task,&info,5000,0) && info.state==XIDE_TASK_CANCELLED);
    CHECK(loadProject(&project,manifest,"echo",&error));
    int wideSize=MultiByteToWideChar(CP_UTF8,0,output,-1,NULL,0); wchar_t* outputWide=malloc((size_t)wideSize*sizeof(*outputWide)); CHECK(outputWide); MultiByteToWideChar(CP_UTF8,0,output,-1,outputWide,wideSize);
    locked=CreateFileW(outputWide,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); free(outputWide); CHECK(locked!=INVALID_HANDLE_VALUE);
    task=xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error); CHECK(task && waitTask(tasks,task,&info,10000,0) && info.state==XIDE_TASK_FAILED && !info.ran && strstr(info.error,"publish")); CloseHandle(locked); locked=INVALID_HANDLE_VALUE;
    CHECK(writeText(manifest,"{}")); task=xideTasksBuild(tasks,&project,"dev",arguments[0],0,&error); CHECK(task && waitTask(tasks,task,&info,10000,0) && info.state==XIDE_TASK_FAILED && strstr(info.error,"outside the loaded model"));
    CHECK(loadProject(&project,manifest,"echo",&error)); CHECK(!xideTasksBuild(tasks,&project,"missing",arguments[0],0,&error));
    CHECK(xideProjectSetMetadata(&project,project.name,project.description,"dll",&error)); CHECK(xideProjectSave(&project,NULL,&error)); CHECK(!xideTasksBuild(tasks,&project,"dev",arguments[0],1,&error) && strstr(error.message,"host"));
    if(count==3) {
        CHECK(writeText(entry,"func main(...) as int\n    print(\"IDE-真实构建运行\")\n    return 0\nend\n"));
        CHECK(loadProject(&project,manifest,"echo",&error)); native=xidePathResolve(root,"native.c"); include=xidePathResolve(root,"include"); library=xidePathResolve(root,"lib"); CHECK(native && include && library && xrtDirCreateAll(include) && xrtDirCreateAll(library));
        CHECK(writeText(native,"int native_answer(void) { return 42; }\n") && xideProjectAddFile(&project,native,"c",&error));
        project.configurations[0].includePaths.items=calloc(1,sizeof(char*)); project.configurations[0].libraryPaths.items=calloc(1,sizeof(char*)); project.configurations[0].linkLibraries.items=calloc(1,sizeof(char*));
        CHECK(project.configurations[0].includePaths.items && project.configurations[0].libraryPaths.items && project.configurations[0].linkLibraries.items);
        project.configurations[0].includePaths.items[0]=xidePathResolve(root,"include"); project.configurations[0].includePaths.count=1; project.configurations[0].libraryPaths.items[0]=xidePathResolve(root,"lib"); project.configurations[0].libraryPaths.count=1;
        project.configurations[0].linkLibraries.items[0]=malloc(7); CHECK(project.configurations[0].linkLibraries.items[0]); strcpy(project.configurations[0].linkLibraries.items[0],"user32"); project.configurations[0].linkLibraries.count=1;
        CHECK(xideProjectSave(&project,NULL,&error)); task=xideTasksBuild(tasks,&project,"dev",arguments[2],1,&error); CHECK(task && waitTask(tasks,task,&info,30000,0)); CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran);
        cursor=0; size=xideTaskReadTerminal(task,&cursor,log,sizeof(log)-1,&dropped); log[size]=0; CHECK(strstr(log,"IDE-真实构建运行"));
        CHECK(writeText(entry,"func main(...) as int\n    不存在的名字()\n    return 0\nend\n")); task=xideTasksBuild(tasks,&project,"dev",arguments[2],1,&error); CHECK(task && waitTask(tasks,task,&info,30000,0)); CHECK(info.state==XIDE_TASK_FAILED && !info.ran && xideTaskDiagnosticCount(task)>0);
        puts("task_test real DEMO6 integration passed: mixed C/xlang, include/library/link flags, Unicode paths, successful run and actual compiler diagnostics.");
    }
    char* outputDirectory=xidePathDirectory(output); char* cache=outputDirectory ? xidePathResolveLexical(outputDirectory,".xlang-ide-build") : NULL; free(outputDirectory); CHECK(cache);
    FILETIME time={0}; GetSystemTimeAsFileTime(&time); uint64_t future=(((uint64_t)time.dwHighDateTime<<32)|time.dwLowDateTime)+XIDE_TASK_STAGE_GRACE_TICKS+UINT64_C(86400)*10000000; xide_task_stage_stats stats={0};
    int kept=xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && !stats.deletedStages; CHECK(kept);
    xideTasksDestroy(tasks); tasks=NULL;
    kept=xideTaskStagePrune(cache,0,future,NULL,NULL,NULL,&stats,&error) && stats.deletedStages>0 && xidePathExists(output); free(cache); CHECK(kept);
    puts("task_test passed: immutable requests, interactive run, queue/coalescing, stale/absent artifact protection, diagnostics, cancellation, invalidation, external manifest, locked destination and history-stage leases released on task destruction.");
cleanup:
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    xideDiagnosticClear(&diagnostic); xideTasksDestroy(tasks); xideProjectClear(&project); xrtFree(bytes); xrtFree(old); free(root); free(manifest); free(entry); free(output); free(include); free(library); free(native); free(alias);
    for(int i=0;i<count;++i) free(arguments[i]);
    free(arguments); return failed;
}
