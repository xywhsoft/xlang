/* 只使用公开 xl.h；不把独立静态包实例误当作真实双上下文执行。 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>
#endif
#include "xl.h"

typedef struct runner {
    XL_Context* context;
    const char* script;
    int result;
#ifdef _WIN32
    volatile LONG done;
    HANDLE thread;
#else
    atomic_int done;
    pthread_t thread;
#endif
    int started;
} runner;
static int completed(runner* job)
{
#ifdef _WIN32
    return InterlockedCompareExchange(&job->done,0,0)!=0;
#else
    return atomic_load(&job->done);
#endif
}
#ifdef _WIN32
static DWORD WINAPI run_script(void* data)
#else
static void* run_script(void* data)
#endif
{
    runner* job=data;
    job->result=xlRunScriptFile(job->context,job->script) && xlGetLastExitCode(job->context)==0;
#ifdef _WIN32
    InterlockedExchange(&job->done,1);return 0;
#else
    atomic_store(&job->done,1);return NULL;
#endif
}
static int start(runner* job)
{
#ifdef _WIN32
    job->thread=CreateThread(NULL,0,run_script,job,0,NULL);
    return job->started=job->thread!=NULL;
#else
    atomic_init(&job->done,0);
    return job->started=pthread_create(&job->thread,NULL,run_script,job)==0;
#endif
}
static void join(runner* job)
{
    if(!job->started)return;
#ifdef _WIN32
    WaitForSingleObject(job->thread,INFINITE);CloseHandle(job->thread);
#else
    pthread_join(job->thread,NULL);
#endif
}
static void pause_step(void)
{
#ifdef _WIN32
    Sleep(10);
#else
    const struct timespec pause={0,10000000};nanosleep(&pause,NULL);
#endif
}
#ifdef _WIN32
static int wide(const char* text,wchar_t* buffer,int capacity)
{
    return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,buffer,capacity)>0;
}
#endif
static int environment(const char* name,const char* value)
{
#ifdef _WIN32
    wchar_t n[128],v[32768];
    return wide(name,n,128) && wide(value,v,32768) && SetEnvironmentVariableW(n,v);
#else
    return setenv(name,value,1)==0;
#endif
}
/* 两个哨兵均在门禁独占目录，不并发读写CRT的进程环境表。 */
static int signal_file(const char* path,int create)
{
#ifdef _WIN32
    wchar_t name[32768];if(!wide(path,name,32768))return 0;
    if(!create)return GetFileAttributesW(name)!=INVALID_FILE_ATTRIBUTES;
    HANDLE file=CreateFileW(name,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    return file!=INVALID_HANDLE_VALUE && CloseHandle(file);
#else
    if(!create)return access(path,F_OK)==0;
    int file=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
    return file>=0 && close(file)==0;
#endif
}
int main(int argc,char** argv)
{
    if(argc!=7)return 2;
    runner hold={0},probe={0};int ready=0,concurrent=0,signalled=0,result=1;
    char name[80];
#ifdef _WIN32
    snprintf(name,sizeof(name),"xl-si-context-%lu",(unsigned long)GetCurrentProcessId());
#else
    snprintf(name,sizeof(name),"xl-si-context-%ld",(long)getpid());
#endif
    if(signal_file(argv[5],0) || signal_file(argv[6],0) ||
        !environment("XLANG_SI_TEST_NAME",name) ||
        !environment("XLANG_SI_TEST_READY_PATH",argv[5]) ||
        !environment("XLANG_SI_TEST_DONE_PATH",argv[6]))return 1;
    XL_ContextOptions options=xlDefaultContextOptions();options.workDir=argv[1];
    hold.context=xlCreateContext(&options);probe.context=xlCreateContext(&options);
    hold.script=argv[2];probe.script=argv[3];
    if(!hold.context || !probe.context || !start(&hold))goto done;
    for(int i=0;i<1000 && !(ready=signal_file(argv[5],0)) && !completed(&hold);++i)pause_step();
    if(!ready || !start(&probe))goto done;
    for(int i=0;i<1000 && !completed(&probe);++i)pause_step();
    concurrent=completed(&probe);
done:
    /* 先通知第一个脚本退出，再join自有线程；外层门禁为异常卡死兜底超时。 */
    if(hold.started)signalled=signal_file(argv[6],1);
    join(&hold);join(&probe);
    if(!ready)fprintf(stderr,"singleinstance holder did not become ready\n");
    else if(!concurrent){
        fprintf(stderr,"blocked-upstream: second XL_Context did not execute during first context's script\n");result=2;
    }else if(signalled && hold.result && probe.result &&
        xlRunScriptFile(probe.context,argv[4]) && xlGetLastExitCode(probe.context)==0){
        puts("singleinstance XL_Context isolation: PASS");result=0;
    }else fprintf(stderr,"singleinstance context result: %s\n",probe.context ? xlGetLastError(probe.context) : "creation failed");
    if(probe.context)xlDestroyContext(probe.context);
    if(hold.context)xlDestroyContext(hold.context);
    return result;
}
